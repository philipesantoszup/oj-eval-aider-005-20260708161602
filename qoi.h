#ifndef QOI_FORMAT_CODEC_QOI_H_
#define QOI_FORMAT_CODEC_QOI_H_

#include "utils.h"

constexpr uint8_t QOI_OP_INDEX_TAG = 0x00;
constexpr uint8_t QOI_OP_DIFF_TAG  = 0x40;
constexpr uint8_t QOI_OP_LUMA_TAG  = 0x80;
constexpr uint8_t QOI_OP_RUN_TAG   = 0xc0; 
constexpr uint8_t QOI_OP_RGB_TAG   = 0xfe;
constexpr uint8_t QOI_OP_RGBA_TAG  = 0xff;
constexpr uint8_t QOI_PADDING[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
constexpr uint8_t QOI_MASK_2 = 0xc0;

/**
 * @brief encode the raw pixel data of an image to qoi format.
 *
 * @param[in] width image width in pixels
 * @param[in] height image height in pixels
 * @param[in] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[in] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace = 0);

/**
 * @brief decode the qoi format of an image to raw pixel data
 *
 * @param[out] width image width in pixels
 * @param[out] height image height in pixels
 * @param[out] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[out] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace);


bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {

    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    QoiWriteU32(width);
    QoiWriteU32(height);
    QoiWriteU8(channels);
    QoiWriteU8(colorspace);

    uint8_t history[64][4] = {0};
    uint8_t pre[4] = {0, 0, 0, 255};
    int run = 0;
    uint64_t px_num = (uint64_t)width * height;

    for (uint64_t i = 0; i < px_num; ++i) {
        uint8_t curr[4];
        curr[0] = QoiReadU8();
        curr[1] = QoiReadU8();
        curr[2] = QoiReadU8();
        curr[3] = (channels == 4) ? QoiReadU8() : 255u;

        if (curr[0] == pre[0] && curr[1] == pre[1] && curr[2] == pre[2] && curr[3] == pre[3]) {
            run++;
            if (run == 63) {
                QoiWriteU8(QOI_OP_RUN_TAG | 63);
                QoiWriteU8(pre[0]);
                QoiWriteU8(pre[1]);
                QoiWriteU8(pre[2]);
                if (channels == 4) QoiWriteU8(pre[3]);
                run = 0;
            }
            continue;
        }

        if (run > 0) {
            QoiWriteU8(QOI_OP_RUN_TAG | (run & 63));
            QoiWriteU8(pre[0]);
            QoiWriteU8(pre[1]);
            QoiWriteU8(pre[2]);
            if (channels == 4) QoiWriteU8(pre[3]);
            run = 0;
        }

        int idx = QoiColorHash(curr[0], curr[1], curr[2], curr[3]);
        if (history[idx][0] == curr[0] && history[idx][1] == curr[1] && 
            history[idx][2] == curr[2] && history[idx][3] == curr[3]) {
            QoiWriteU8(QOI_OP_INDEX_TAG | (idx & 63));
        } else {
            uint8_t dr = curr[0] - pre[0];
            uint8_t dg = curr[1] - pre[1];
            uint8_t db = curr[2] - pre[2];
            uint8_t da = curr[3] - pre[3];

            bool fits_diff = (dr <= 64 || dr >= 192) && (dg <= 64 || dg >= 192) && (db <= 64 || db >= 192);
            if (channels == 4) {
                fits_diff = fits_diff && (da <= 64 || da >= 192);
            }

            if (fits_diff) {
                QoiWriteU8(QOI_OP_DIFF_TAG | (dr & 63));
                QoiWriteU8(dr);
                QoiWriteU8(dg);
                QoiWriteU8(db);
                if (channels == 4) QoiWriteU8(da);
            } else {
                if (dr == (uint8_t)((dg * 4) & 0xFF) && db == (uint8_t)((dg * 2) & 0xFF) && (dg <= 64 || dg >= 192)) {
                    QoiWriteU8(QOI_OP_LUMA_TAG | (dg & 63));
                    QoiWriteU8(dg);
                } else {
                    if (channels == 3) {
                        QoiWriteU8(QOI_OP_RGB_TAG);
                        QoiWriteU8(curr[0]); QoiWriteU8(curr[1]); QoiWriteU8(curr[2]);
                    } else {
                        QoiWriteU8(QOI_OP_RGBA_TAG);
                        QoiWriteU8(curr[0]); QoiWriteU8(curr[1]); QoiWriteU8(curr[2]); QoiWriteU8(curr[3]);
                    }
                }
            }
        }

        history[idx][0] = curr[0];
        history[idx][1] = curr[1];
        history[idx][2] = curr[2];
        history[idx][3] = curr[3];
        pre[0] = curr[0];
        pre[1] = curr[1];
        pre[2] = curr[2];
        pre[3] = curr[3];
    }

    if (run > 0) {
        QoiWriteU8(QOI_OP_RUN_TAG | (run & 63));
        QoiWriteU8(pre[0]);
        QoiWriteU8(pre[1]);
        QoiWriteU8(pre[2]);
        if (channels == 4) QoiWriteU8(pre[3]);
    }

    for (int i = 0; i < 8; ++i) {
        QoiWriteU8(QOI_PADDING[i]);
    }

    return true;
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {
    char magic[4];
    magic[0] = QoiReadChar();
    magic[1] = QoiReadChar();
    magic[2] = QoiReadChar();
    magic[3] = QoiReadChar();
    if (magic[0] != 'q' || magic[1] != 'o' || magic[2] != 'i' || magic[3] != 'f') {
        return false;
    }

    width = QoiReadU32();
    height = QoiReadU32();
    channels = QoiReadU8();
    colorspace = QoiReadU8();

    uint8_t history[64][4] = {0};
    uint8_t pre[4] = {0, 0, 0, 255};
    uint64_t px_num = (uint64_t)width * height;
    uint64_t decoded_px = 0;

    while (decoded_px < px_num) {
        uint8_t tag = QoiReadU8();
        uint8_t curr[4] = {0};

        if ((tag & QOI_MASK_2) == QOI_OP_RUN_TAG) {
            int run = tag & 63;
            if (run == 0) run = 63;
            uint8_t r = QoiReadU8();
            uint8_t g = QoiReadU8();
            uint8_t b = QoiReadU8();
            uint8_t a = (channels == 4) ? QoiReadU8() : 255u;
            
            for (int j = 0; j < run && decoded_px < px_num; ++j) {
                QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b);
                if (channels == 4) QoiWriteU8(a);
                decoded_px++;
            }
            pre[0] = r; pre[1] = g; pre[2] = b; pre[3] = a;
            int idx = QoiColorHash(r, g, b, a);
            history[idx][0] = r; history[idx][1] = g; history[idx][2] = b; history[idx][3] = a;
            continue;
        } else if ((tag & QOI_MASK_2) == QOI_OP_INDEX_TAG) {
            int idx = tag & 63;
            curr[0] = history[idx][0];
            curr[1] = history[idx][1];
            curr[2] = history[idx][2];
            curr[3] = history[idx][3];
        } else if ((tag & QOI_MASK_2) == QOI_OP_DIFF_TAG) {
            uint8_t dr = QoiReadU8();
            uint8_t dg = QoiReadU8();
            uint8_t db = QoiReadU8();
            curr[0] = pre[0] + dr;
            curr[1] = pre[1] + dg;
            curr[2] = pre[2] + db;
            if (channels == 4) curr[3] = pre[3] + QoiReadU8();
            else curr[3] = 255u;
        } else if ((tag & QOI_MASK_2) == QOI_OP_LUMA_TAG) {
            uint8_t dg = QoiReadU8();
            curr[0] = pre[0] + (uint8_t)((dg * 4) & 0xFF);
            curr[1] = pre[1] + dg;
            curr[2] = pre[2] + (uint8_t)((dg * 2) & 0xFF);
            curr[3] = pre[3];
        } else if (tag == QOI_OP_RGB_TAG) {
            curr[0] = QoiReadU8();
            curr[1] = QoiReadU8();
            curr[2] = QoiReadU8();
            curr[3] = 255u;
        } else if (tag == QOI_OP_RGBA_TAG) {
            curr[0] = QoiReadU8();
            curr[1] = QoiReadU8();
            curr[2] = QoiReadU8();
            curr[3] = QoiReadU8();
        } else {
            return false;
        }

        QoiWriteU8(curr[0]);
        QoiWriteU8(curr[1]);
        QoiWriteU8(curr[2]);
        if (channels == 4) QoiWriteU8(curr[3]);
        decoded_px++;

        int idx = QoiColorHash(curr[0], curr[1], curr[2], curr[3]);
        history[idx][0] = curr[0];
        history[idx][1] = curr[1];
        history[idx][2] = curr[2];
        history[idx][3] = curr[3];
        pre[0] = curr[0];
        pre[1] = curr[1];
        pre[2] = curr[2];
        pre[3] = curr[3];
    }

    for (int i = 0; i < 8; ++i) {
        if (QoiReadU8() != QOI_PADDING[i]) return false;
    }

    return true;
}

#endif // QOI_FORMAT_CODEC_QOI_H_
