#include "ImageHistTools.h"

#include <QPainter>
#include <QtMath>

#include <vector>
#include <algorithm>

QPixmap ImageHistTools::drawGrayHistogram(const QImage &img, int w, int h)
{
    if (w <= 0 || h <= 0) {
        return {};
    }

    QImage g = img.convertToFormat(QImage::Format_Grayscale8);
    std::vector<int> hist(256, 0);

    for (int y = 0; y < g.height(); y++) {
        const uchar *p = g.constScanLine(y);
        for (int x = 0; x < g.width(); x++) hist[p[x]]++;
    }

    int max = *std::max_element(hist.begin(), hist.end());
    if (max <= 0) {
        max = 1;
    }

    QPixmap pix(w, h);
    pix.fill(Qt::white);
    QPainter pp(&pix);

    const int left = 18;
    const int right = 10;
    const int top = 10;
    const int bottom = 18;
    const int plotW = std::max(1, w - left - right);
    const int plotH = std::max(1, h - top - bottom);
    const int baseY = h - bottom;

    pp.setPen(QPen(QColor("#94a3b8"), 1));
    pp.drawLine(left, baseY, w - right, baseY);
    pp.drawLine(left, top, left, baseY);

    pp.setPen(QPen(QColor("#1d4ed8"), 1));
    for (int i = 0; i < 256; i++) {
        int barH = hist[i] * plotH / max;
        int x = left + (i * plotW) / 256;
        int y1 = baseY;
        int y2 = baseY - barH;
        pp.drawLine(x, y1, x, y2);
    }
    return pix;
}

QImage ImageHistTools::linearStretch(const QImage &img)
{
    QImage g = img.convertToFormat(QImage::Format_Grayscale8);
    int min = 255;
    int max = 0;

    for (int y = 0; y < g.height(); y++) {
        const uchar *p = g.constScanLine(y);
        for (int x = 0; x < g.width(); x++) {
            int val = static_cast<int>(p[x]);
            if (val < min) min = val;
            if (val > max) max = val;
        }
    }

    if (min == max) return g;

    QImage out(g.size(), QImage::Format_Grayscale8);
    double a = 255.0 / (max - min);
    for (int y = 0; y < g.height(); y++) {
        const uchar *s = g.constScanLine(y);
        uchar *d = out.scanLine(y);
        for (int x = 0; x < g.width(); x++) {
            int value = static_cast<int>((s[x] - min) * a);
            d[x] = static_cast<uchar>(qBound(0, value, 255));
        }
    }
    return out;
}

QImage ImageHistTools::equalizeHist(const QImage &img)
{
    QImage gray = img.convertToFormat(QImage::Format_Grayscale8);
    int width = gray.width();
    int height = gray.height();

    QImage out(width, height, QImage::Format_Grayscale8);
    out.fill(Qt::black);

    for (int y = 0; y < height; y++) {
        const uchar *srcLine = gray.constScanLine(y);
        uchar *dstLine = out.scanLine(y);

        for (int x = 0; x < width; x++) {
            int x1 = qMax(0, x - 1);
            int y1 = qMax(0, y - 1);
            int x2 = qMin(width - 1, x + 1);
            int y2 = qMin(height - 1, y + 1);

            std::vector<int> hist(256, 0);
            int localTotal = 0;
            for (int yy = y1; yy <= y2; yy++) {
                const uchar *line = gray.constScanLine(yy);
                for (int xx = x1; xx <= x2; xx++) {
                    hist[line[xx]]++;
                    localTotal++;
                }
            }

            std::vector<int> cdf(256, 0);
            cdf[0] = hist[0];
            for (int i = 1; i < 256; i++) {
                cdf[i] = cdf[i-1] + hist[i];
            }

            uchar oldVal = srcLine[x];
            uchar newVal = static_cast<uchar>(cdf[oldVal] * 255 / localTotal);
            dstLine[x] = newVal;
        }
    }

    return out;
}

QImage ImageHistTools::matchHist(const QImage &src, const QImage &target)
{
    if (src.isNull() || target.isNull()) return src;

    // 转换为32位彩色图像，方便处理RGB通道
    QImage srcImg = src.convertToFormat(QImage::Format_RGB32);
    QImage tgtImg = target.convertToFormat(QImage::Format_RGB32);
    QImage result(srcImg.size(), QImage::Format_RGB32);

    // 分别对R、G、B三个通道做直方图匹配
    for (int channel = 0; channel < 3; channel++) {
        // 1. 统计源图像和目标图像的直方图
        std::vector<int> srcHist(256, 0);
        std::vector<int> tgtHist(256, 0);

        for (int y = 0; y < srcImg.height(); y++) {
            const QRgb* line = reinterpret_cast<const QRgb*>(srcImg.constScanLine(y));
            for (int x = 0; x < srcImg.width(); x++) {
                int val;
                if (channel == 0) val = qRed(line[x]);
                else if (channel == 1) val = qGreen(line[x]);
                else val = qBlue(line[x]);
                srcHist[val]++;
            }
        }

        for (int y = 0; y < tgtImg.height(); y++) {
            const QRgb* line = reinterpret_cast<const QRgb*>(tgtImg.constScanLine(y));
            for (int x = 0; x < tgtImg.width(); x++) {
                int val;
                if (channel == 0) val = qRed(line[x]);
                else if (channel == 1) val = qGreen(line[x]);
                else val = qBlue(line[x]);
                tgtHist[val]++;
            }
        }

        // 2. 计算累积分布函数CDF
        int srcTotal = srcImg.width() * srcImg.height();
        int tgtTotal = tgtImg.width() * tgtImg.height();

        std::vector<double> srcCdf(256, 0.0);
        std::vector<double> tgtCdf(256, 0.0);

        srcCdf[0] = srcHist[0] / (double)srcTotal;
        tgtCdf[0] = tgtHist[0] / (double)tgtTotal;
        for (int i = 1; i < 256; i++) {
            srcCdf[i] = srcCdf[i-1] + srcHist[i] / (double)srcTotal;
            tgtCdf[i] = tgtCdf[i-1] + tgtHist[i] / (double)tgtTotal;
        }

        // 3. 构建映射表
        std::vector<uchar> map(256, 0);
        for (int i = 0; i < 256; i++) {
            double diff = 1.0;
            int idx = 0;
            for (int j = 0; j < 256; j++) {
                if (fabs(srcCdf[i] - tgtCdf[j]) < diff) {
                    diff = fabs(srcCdf[i] - tgtCdf[j]);
                    idx = j;
                }
            }
            map[i] = static_cast<uchar>(idx);
        }

        // 4. 对当前通道做映射
        for (int y = 0; y < srcImg.height(); y++) {
            const QRgb* srcLine = reinterpret_cast<const QRgb*>(srcImg.constScanLine(y));
            QRgb* resLine = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = 0; x < srcImg.width(); x++) {
                int val;
                if (channel == 0) val = qRed(srcLine[x]);
                else if (channel == 1) val = qGreen(srcLine[x]);
                else val = qBlue(srcLine[x]);
                uchar newVal = map[val];
                if (channel == 0) resLine[x] = qRgb(newVal, qGreen(resLine[x]), qBlue(resLine[x]));
                else if (channel == 1) resLine[x] = qRgb(qRed(resLine[x]), newVal, qBlue(resLine[x]));
                else resLine[x] = qRgb(qRed(resLine[x]), qGreen(resLine[x]), newVal);
            }
        }
    }

    return result;
}
