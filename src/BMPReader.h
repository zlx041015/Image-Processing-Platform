#pragma once

#include <QString>
#include <QImage>
#include <QVector>
#include <QColor>
#include <QByteArray>

#include <cstdint>
#include <cstring>

//定义BMPReader类，用于读取和解码BMP图像文件
class BMPReader {
public:
    struct Info {   //定义Info结构体，包含BMP图像的相关信息,初始化
        int width = 0;
        int height = 0;
        bool topDown = false;   //图片正反
        int bitCount = 0;   //每像素位数
        uint32_t compression = 0;   //压缩类型
        uint32_t colorsUsed = 0;    //调色板颜色数
        uint32_t dibSize = 0;   //DIB头大小
        QString fileName;   //文件名
    };

    explicit BMPReader(const QString& filePath);    //构造函数，接受BMP文件路径作为参数

    bool load(QString* errorMessage = nullptr); //加载BMP文件
    QImage decode(QString* errorMessage = nullptr); //解码BMP图像数据，返回QImage对象(将数据变为可显示的图片)
    const Info& info() const { return m_info; } //获取BMP图像信息

private:
    struct Masks {  //定义Masks结构体，包含BMP图像的颜色掩码,初始化
        uint32_t r = 0;
        uint32_t g = 0;
        uint32_t b = 0;
        uint32_t a = 0;
    };

    QString m_filePath; //BMP文件路径
    QByteArray m_data;  //BMP文件数据
    Info m_info;    //BMP图像信息
    uint32_t m_pixelOffset = 0; //像素数据偏移
    QVector<QColor> m_palette; //调色板
    bool m_hasMasks = false;    //是否有颜色掩码
    Masks m_masks;  //颜色掩码

    bool readFile(QString* errorMessage);   //读取BMP文件数据
    bool readHeaders(QString* errorMessage);    //读取BMP文件头和DIB头，解析图像信息
    void readMasksAndPalette(); //读取颜色掩码和调色板

    static int rowSize(int bitsPerRow); //计算每行像素数据的字节数
    static uint8_t applyMask(uint32_t value, uint32_t mask);    //应用颜色掩码，提取颜色分量

    bool ensureAvailable(int offset, int count) const;  //确保在数据中有足够的字节可供读取
    template<typename T>    //从数据中以小端格式读取一个值(数据是倒着存的，反过来读取）
    T readLE(int offset) const {    //定义一个模板函数，接受一个偏移量参数
        T value{};//初始化一个值
        std::memcpy(&value, m_data.constData() + offset, sizeof(T));    //从第offset个字节开始复制T字节数到value变量中
        return value;   //cpu小端存储，直接读取
    }
};
