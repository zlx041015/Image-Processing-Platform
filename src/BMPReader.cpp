#include "BMPReader.h"

#include <QFile>
#include <QFileInfo>
#include <cstring>
#include <vector>

//定义BMP标准压缩类型，只解析这三种合法格式
namespace {
constexpr uint32_t BI_RGB = 0;  //无压缩
constexpr uint32_t BI_BITFIELDS = 3;    //R/G/B掩码
constexpr uint32_t BI_ALPHABITFIELDS = 6;   //R/G/B/A掩码
}

//接收图片路径，读取文件，解析头部信息，提取调色板和掩码，并最终解码成QImage对象供显示使用
BMPReader::BMPReader(const QString& filePath) : m_filePath(filePath) {  //保存路径
    m_info.fileName = QFileInfo(filePath).fileName();  //记录图片信息
}

//加载BMP文件，读取文件内容，解析头部信息，提取调色板和掩码等元数据，为解码做准备
bool BMPReader::load(QString* errorMessage) {
    m_palette.clear();  //清空调色板
    m_hasMasks = false; //重置掩码标志
    m_masks = {};   //重置掩码数据(存红蓝黄)
    m_pixelOffset = 0;  //颜色数据起点
    m_info = {};    //清空信息，重新记录图片信息
    m_info.fileName = QFileInfo(m_filePath).fileName(); //记录文件名

    //打开文件，读成二进制
    if (!readFile(errorMessage)) {
        return false;
    }
    //读文件头，提取图像尺寸、位深、压缩类型等信息
    if (!readHeaders(errorMessage)) {
        return false;
    }
    //根据头部信息，提取调色板和掩码等元数据
    readMasksAndPalette();
    return true;
}

//QImage对象是Qt框架中用于处理图像数据的类，BMPReader::decode方法将BMP文件解码成QImage对象
QImage BMPReader::decode(QString* errorMessage) {
    if (m_data.isEmpty()) { //图片的二进制数据
        if (!load(errorMessage)) {
            return {};
        }
    }

    //三种合法的压缩格式
    if (m_info.compression != BI_RGB &&
        m_info.compression != BI_BITFIELDS &&
        m_info.compression != BI_ALPHABITFIELDS) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("暂不支持该压缩格式");
        }
        return {};
    }

    //image:要生成的图片；图片高度、宽度和格式（RGBA各8位）
    QImage image(m_info.width, m_info.height, QImage::Format_RGBA8888);
    if (image.isNull()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建图像缓冲区");
        }
        return {};
    }

    //绘图过程，将像素点填入图片
    auto putPixel = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        auto* line = image.scanLine(y);
        int idx = x * 4;
        line[idx + 0] = r;
        line[idx + 1] = g;
        line[idx + 2] = b;
        line[idx + 3] = a;
    };

    const int width = m_info.width;
    const int height = m_info.height;

    //根据不同的位深对图片进行处理
    if (m_info.bitCount == 1) {
        const int rowBytes = rowSize(width);    //每行占用的字节数（4字节对齐）
        for (int row = 0; row < height; ++row) {    //BMP文件中像素数据的行是从下往上存储的
            int srcY = m_info.topDown ? row : (height - 1 - row);   //计算当前行在文件中的位置
            int rowStart = static_cast<int>(m_pixelOffset) + srcY * rowBytes;   //每行的读取数据起点
            for (int x = 0; x < width; ++x) {   //每个像素占1位，8个像素共占1字节
                int byteIndex = x / 8;  //计算当前像素所在字节的索引
                int bitIndex = 7 - (x % 8); //计算当前像素在字节中的位索引（从高位到低位）
                uint8_t byteValue = static_cast<uint8_t>(m_data[rowStart + byteIndex]); //读取该字节的值
                int paletteIndex = (byteValue >> bitIndex) & 0x01;  //提取对应位的值作为调色板索引
                QColor c = m_palette.value(paletteIndex, QColor(0, 0, 0, 255)); //根据索引获取颜色，默认黑色
                putPixel(x, row, c.red(), c.green(), c.blue(), c.alpha());  //将颜色值写入图片
            }
        }
    } else if (m_info.bitCount == 4) {
        const int rowBytes = rowSize(width * 4);
        for (int row = 0; row < height; ++row) {
            int srcY = m_info.topDown ? row : (height - 1 - row);
            int rowStart = static_cast<int>(m_pixelOffset) + srcY * rowBytes;
            for (int x = 0; x < width; ++x) {
                int byteIndex = x / 2;
                uint8_t byteValue = static_cast<uint8_t>(m_data[rowStart + byteIndex]);
                int paletteIndex = (x % 2 == 0) ? ((byteValue >> 4) & 0x0F) : (byteValue & 0x0F);
                QColor c = m_palette.value(paletteIndex, QColor(0, 0, 0, 255));
                putPixel(x, row, c.red(), c.green(), c.blue(), c.alpha());
            }
        }
    } else if (m_info.bitCount == 8) {
        const int rowBytes = rowSize(width * 8);
        for (int row = 0; row < height; ++row) {
            int srcY = m_info.topDown ? row : (height - 1 - row);
            int rowStart = static_cast<int>(m_pixelOffset) + srcY * rowBytes;
            for (int x = 0; x < width; ++x) {
                int paletteIndex = static_cast<uint8_t>(m_data[rowStart + x]);
                QColor c = m_palette.value(paletteIndex, QColor(0, 0, 0, 255));
                putPixel(x, row, c.red(), c.green(), c.blue(), c.alpha());
            }
        }
    } else if (m_info.bitCount == 16) {
        const int rowBytes = rowSize(width * 16);
        Masks masks = m_hasMasks ? m_masks : Masks{0x7C00, 0x03E0, 0x001F, 0x0000};
        for (int row = 0; row < height; ++row) {
            int srcY = m_info.topDown ? row : (height - 1 - row);
            int rowStart = static_cast<int>(m_pixelOffset) + srcY * rowBytes;
            for (int x = 0; x < width; ++x) {
                uint16_t value = readLE<uint16_t>(rowStart + x * 2);
                uint8_t r = applyMask(value, masks.r);
                uint8_t g = applyMask(value, masks.g);
                uint8_t b = applyMask(value, masks.b);
                uint8_t a = masks.a ? applyMask(value, masks.a) : 255;
                putPixel(x, row, r, g, b, a);
            }
        }
    } else if (m_info.bitCount == 24) {
        const int rowBytes = rowSize(width * 24);
        for (int row = 0; row < height; ++row) {
            int srcY = m_info.topDown ? row : (height - 1 - row);
            int rowStart = static_cast<int>(m_pixelOffset) + srcY * rowBytes;
            for (int x = 0; x < width; ++x) {
                int off = rowStart + x * 3;
                uint8_t b = static_cast<uint8_t>(m_data[off + 0]);
                uint8_t g = static_cast<uint8_t>(m_data[off + 1]);
                uint8_t r = static_cast<uint8_t>(m_data[off + 2]);
                putPixel(x, row, r, g, b, 255);
            }
        }
    } else if (m_info.bitCount == 32) {
        const int rowBytes = rowSize(width * 32);
        for (int row = 0; row < height; ++row) {
            int srcY = m_info.topDown ? row : (height - 1 - row);
            int rowStart = static_cast<int>(m_pixelOffset) + srcY * rowBytes;
            for (int x = 0; x < width; ++x) {
                int off = rowStart + x * 4;
                if (m_hasMasks) {
                    uint32_t value = readLE<uint32_t>(off);
                    uint8_t r = applyMask(value, m_masks.r);
                    uint8_t g = applyMask(value, m_masks.g);
                    uint8_t b = applyMask(value, m_masks.b);
                    uint8_t a = m_masks.a ? applyMask(value, m_masks.a) : 255;
                    putPixel(x, row, r, g, b, a);
                } else {
                    uint8_t b = static_cast<uint8_t>(m_data[off + 0]);
                    uint8_t g = static_cast<uint8_t>(m_data[off + 1]);
                    uint8_t r = static_cast<uint8_t>(m_data[off + 2]);
                    uint8_t a = static_cast<uint8_t>(m_data[off + 3]);
                    putPixel(x, row, r, g, b, a);
                }
            }
        }
    } else {
        if (errorMessage) {
            *errorMessage = QStringLiteral("暂不支持该 BMP 位深");
        }
        return {};
    }

    return image;
}

//打开文件，读成二进制数据，检查文件头和信息头大小是否至少54字节
bool BMPReader::readFile(QString* errorMessage) {
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法打开文件: ") + file.errorString();
        }
        return false;
    }
    m_data = file.readAll();
    if (m_data.size() < 54) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("文件太小，不是有效 BMP");
        }
        return false;
    }
    return true;
}

//读文件头，提取图像尺寸、位深、压缩类型等信息，检查合法性
bool BMPReader::readHeaders(QString* errorMessage) {
    if (m_data[0] != 'B' || m_data[1] != 'M') {
        if (errorMessage) {
            *errorMessage = QStringLiteral("不是有效的 BMP 文件");
        }
        return false;
    }

    m_pixelOffset = readLE<uint32_t>(10);   //颜色数据起点
    m_info.dibSize = readLE<uint32_t>(14);  //DIB头大小

    if (m_info.dibSize < 40) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("暂不支持旧格式的 DIB 头格式");
        }
        return false;
    }

    //提取图像尺寸、位深、压缩类型等信息，数字是地址偏移量，每一块存不同的信息
    int32_t width = readLE<int32_t>(18);
    int32_t height = readLE<int32_t>(22);
    uint16_t planes = readLE<uint16_t>(26);
    uint16_t bitCount = readLE<uint16_t>(28);
    uint32_t compression = readLE<uint32_t>(30);
    uint32_t colorsUsed = readLE<uint32_t>(46);

    //检查合法性，BMP文件必须满足这些条件(planes必须为1)
    if (planes != 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("BMP 的 biPlanes 必须为 1");
        }
        return false;
    }

    //存储读取的信息到成员变量中，供后续使用
    m_info.width = width;
    m_info.topDown = height < 0;
    m_info.height = std::abs(height);
    m_info.bitCount = bitCount;
    m_info.compression = compression;
    m_info.colorsUsed = colorsUsed;

    //检查图像尺寸是否合法，宽高必须为正数
    if (m_info.width <= 0 || m_info.height <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无效的图像尺寸");
        }
        return false;
    }

    return true;
}

//根据头部信息，提取调色板和掩码等元数据
void BMPReader::readMasksAndPalette() {
    int pos = 14 + static_cast<int>(m_info.dibSize);

    if ((m_info.compression == BI_BITFIELDS || m_info.compression == BI_ALPHABITFIELDS) &&
        (m_info.bitCount == 16 || m_info.bitCount == 32)) {
        int remaining = static_cast<int>(m_pixelOffset) - pos;
        if (remaining >= 12) {
            m_hasMasks = true;
            m_masks.r = readLE<uint32_t>(pos + 0);
            m_masks.g = readLE<uint32_t>(pos + 4);
            m_masks.b = readLE<uint32_t>(pos + 8);
            pos += 12;
            if (remaining >= 16) {
                m_masks.a = readLE<uint32_t>(pos);
                pos += 4;
            }
        }
    }

    if (m_info.bitCount == 1 || m_info.bitCount == 4 || m_info.bitCount == 8) {
        int paletteSize = m_info.colorsUsed != 0 ? static_cast<int>(m_info.colorsUsed) : (1 << m_info.bitCount);
        m_palette.reserve(paletteSize);
        for (int i = 0; i < paletteSize; ++i) {
            int off = pos + i * 4;
            if (!ensureAvailable(off, 4)) {
                break;
            }
            uint8_t b = static_cast<uint8_t>(m_data[off + 0]);
            uint8_t g = static_cast<uint8_t>(m_data[off + 1]);
            uint8_t r = static_cast<uint8_t>(m_data[off + 2]);
            m_palette.push_back(QColor(r, g, b, 255));
        }
    }
}

//一行是4字节的整数倍，不足部分用0填充
int BMPReader::rowSize(int bitsPerRow) {
    return ((bitsPerRow + 31) / 32) * 4;
}

//根据掩码提取颜色分量，并将其缩放到0-255范围
uint8_t BMPReader::applyMask(uint32_t value, uint32_t mask) {
    if (mask == 0) {
        return 255;
    }

    int shift = 0;
    uint32_t tempMask = mask;
    while ((tempMask & 1U) == 0U) {
        tempMask >>= 1U;
        ++shift;
    }

    uint32_t maxVal = tempMask;
    uint32_t raw = (value & mask) >> shift;
    if (maxVal == 0) {
        return 0;
    }
    return static_cast<uint8_t>(raw * 255 / maxVal);
}

//检查数据范围，确保读取操作不会越界
bool BMPReader::ensureAvailable(int offset, int count) const {
    return offset >= 0 && count >= 0 && offset + count <= m_data.size();
}
