/************************************************************************************************************************
����QOI��ʽ��ͼ�������㷨(Cģ��/�ƽ�ο�)
@info  ��лQOI��Ŀ������(Dominic Szablewski)
@attention ��
@date   2023/03/07
@author Dominic Szablewski �¼�ҫ
@eidt   2023/03/07 �¼�ҫ 0.10 �ı���QOI��Ŀ, ����Ϊ�����ڱ����̵ı�����㷨
		2023/03/19 �¼�ҫ 0.20 ������DIFF2/DIFF3�׶�, ������DIFF/DIFF2/DIFF3/LUMA�׶ο�ѡ��Ԥ��������ģʽ
		2023/03/27 �¼�ҫ 0.30 �����е��з���������Ϊ�޷�����(����Ԥ��������׶�), �����̶Ȼ�ԭӲ������ʱ�����
		2023/04/08 �¼�ҫ 0.40 ������alphaͨ�������
		2023/04/12 �¼�ҫ 0.40 �޸���alphaͨ�������(��byteΪ��λ�ı������, ��4��alpha����Ϊ����ԭ�ӵ�Ԫ)
************************************************************************************************************************/

#include "enhanced_qoi.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////

// rgb���ص�(�ṹ�嶨��)
typedef struct {
	unsigned char r, g, b;
} qoi_rgb_t;

// alpha������(�ṹ�嶨��)
typedef struct {
	unsigned char a1, a2, a3, a4;
} qoi_alpha_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////

// QOI���в���
#define MAX_RUN 31 // RGB�����γ̳���(����<=31)
#define INDEX_TB_L 32 // ��������(����<=32)
#define QOI_COLOR_HASH(C) (C.r + C.g + C.b) // RGB��ϣ����

// RGB����ģʽ��־
#define QOI_OP_INDEX  0x00 /* 000xxxxx */
#define QOI_OP_DIFF3  0x20 /* 001xxxxx */
#define QOI_OP_DIFF   0x40 /* 01xxxxxx */
#define QOI_OP_LUMA   0x80 /* 10xxxxxx */
#define QOI_OP_DIFF2   0xc0 /* 110xxxxx */
#define QOI_OP_RUN    0xe0 /* 111xxxxx */
#define QOI_OP_RGB    0xff /* 11111111 */

// RGB����ʱ�Ĳ�������
#define QOI_MASK_2    0xc0 /* 11000000 */
#define QOI_MASK_3    0xe0 /* 11100000 */

////////////////////////////////////////////////////////////////////////////////////////////////////////////

_Bool init_predict_iter(int w, qoi_rgb_t* predict); // ��ʼ��Ԥ�������
void get_next_predict_v(qoi_rgb_t* rgb, qoi_rgb_t* predict, int* predict_err); // ��ȡ��һ��Ԥ��ֵ
void clear_predict_iter(void); // ����Ԥ�������

////////////////////////////////////////////////////////////////////////////////////////////////////////////

qoi_rgb_t index_tb[INDEX_TB_L]; // ������
qoi_rgb_t px; // ��ǰ����
qoi_rgb_t px_prev; // ��һ������

// Ԥ����������
qoi_rgb_t rgb_pre; // ��������һ������
qoi_rgb_t* rgb_pre_line; // ��������һ�е�����(�׵�ַ)
int predict_w; // ���������Ԥ��ͼƬ�Ŀ��
_Bool predict_decode_first_line; // ������λ�ڵ�һ��(��־)
int predict_column_i; // ��������ǰ���б��

////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*************************
@encode
@public
@brief  ��ͼ�����QOI����
@param  prgb ��������(ָ��)
		pCompressed ѹ�����ݻ�����(ָ��)
		img_w ͼ����
		img_h ͼ��߶�
@return ѹ�����ֽ���
*************************/
int enhanced_qoi_encode(unsigned char* prgb, unsigned char* pCompressed, int img_w, int img_h) {
	memset(index_tb, 0, INDEX_TB_L * sizeof(qoi_rgb_t));
	px_prev = (qoi_rgb_t){ 0, 0, 0 };

	int p = 0;
	int run = 0;
	int img_len = img_w * img_h * 3;
	int px_end = img_len - 4;

	int predict_err[3] = { 0, 0, 0 };
	qoi_rgb_t pix_predict;

	init_predict_iter(img_w, &pix_predict);

	for (int px_pos = 0; px_pos < img_len; px_pos += 3) {
		px = (qoi_rgb_t){ prgb[px_pos + 2], prgb[px_pos + 1], prgb[px_pos] };

		// ����Ԥ�����
		predict_err[0] = px.r - pix_predict.r;
		predict_err[1] = px.g - pix_predict.g;
		predict_err[2] = px.b - pix_predict.b;

		if (!memcmp(&px, &px_prev, sizeof(qoi_rgb_t))) {
			run++;
			if (run == MAX_RUN || px_pos == px_end) {
				// 3'b111 RUN[4:0]-1
				pCompressed[p++] = QOI_OP_RUN | (run - 1);
				run = 0;
			}
		}
		else {
			unsigned char index_pos = QOI_COLOR_HASH(px) % INDEX_TB_L;

			if (run) {
				// 3'b111 RUN[4:0]-1
				pCompressed[p++] = QOI_OP_RUN | (run - 1);
				run = 0;
			}

			if (!memcmp(index_tb + index_pos, &px, sizeof(qoi_rgb_t))) {
				// 3'b000 index[4:0]
				pCompressed[p++] = QOI_OP_INDEX | index_pos;
			}
			else {
				unsigned char vr = px.r - pix_predict.r;
				unsigned char vg = px.g - pix_predict.g;
				unsigned char vb = px.b - pix_predict.b;

				unsigned char vg_r = vr - vg;
				unsigned char vg_b = vb - vg;

				if (((vr & 0xfe) == 0xfe || (vr & 0xfe) == 0x00) &&
					((vg & 0xfe) == 0xfe || (vg & 0xfe) == 0x00) &&
					((vb & 0xfe) == 0xfe || (vb & 0xfe) == 0x00)) {
					vr &= 0x03;
					vg &= 0x03;
					vb &= 0x03;

					// 2'b01 vr[1:0] vg[1:0] vb[1:0]
					pCompressed[p++] = QOI_OP_DIFF | (vr << 4) | (vg << 2) | vb;
				}
				else if (((vr & 0xf8) == 0xf8 || (vr & 0xf8) == 0x00) &&
					((vg & 0xf0) == 0xf0 || (vg & 0xf0) == 0x00) &&
					((vb & 0xf8) == 0xf8 || (vb & 0xf8) == 0x00)) {
					vr &= 0x0f;
					vg &= 0x1f;
					vb &= 0x0f;

					// 3'b001 vg[4:0]
					pCompressed[p++] = QOI_OP_DIFF3 | vg;
					// vr[3:0] vb[3:0]
					pCompressed[p++] = (vr << 4) | vb;
				}
				else if (((vg_r & 0xf8) == 0xf8 || (vg_r & 0xf8) == 0x00) &&
					((vg_b & 0xf8) == 0xf8 || (vg_b & 0xf8) == 0x00) &&
					((vg & 0xe0) == 0xe0 || (vg & 0xe0) == 0x00)) {
					vg_r &= 0x0f;
					vg_b &= 0x0f;
					vg &= 0x3f;

					// 2'b10 vg[5:0]
					pCompressed[p++] = QOI_OP_LUMA | vg;
					// vg_r[3:0] vg_b[3:0]
					pCompressed[p++] = (vg_r << 4) | vg_b;
				}
				else if (((vr & 0xc0) == 0xc0 || (vr & 0xc0) == 0x00) &&
					((vg & 0xc0) == 0xc0 || (vg & 0xc0) == 0x00) &&
					((vb & 0xc0) == 0xc0 || (vb & 0xc0) == 0x00)) {
					vr &= 0x7f;
					vg &= 0x7f;
					vb &= 0x7f;

					// 3'b110 vr[4:0]
					pCompressed[p++] = QOI_OP_DIFF2 | (vr & 0x1f);
					// vg[5:0] vr[6:5]
					pCompressed[p++] = (vr >> 5) | ((vg & 0x3f) << 2);
					// vb[6:0] vg[6]
					pCompressed[p++] = ((vg & 0x40) >> 6) | (vb << 1);
				}
				else {
					// 8'hff
					pCompressed[p++] = QOI_OP_RGB;
					// r[7:0]
					pCompressed[p++] = px.r;
					// g[7:0]
					pCompressed[p++] = px.g;
					// b[7:0]
					pCompressed[p++] = px.b;
				}
			}
			index_tb[index_pos] = px;
		}
		px_prev = px;

		get_next_predict_v(&px, &pix_predict, predict_err);
	}

	clear_predict_iter();

	return p;
}

/*************************
@decode
@public
@brief  ��ͼ�����QOI����
@param  pencoded ѹ������(ָ��)
		pdecoded ���뻺����(ָ��)
		img_w ͼ����
		img_h ͼ��߶�
@return none
*************************/
void enhanced_qoi_decode(unsigned char* pencoded, unsigned char* pdecoded, int img_w, int img_h) {
	int px_len = img_w * img_h * 3;

	memset(index_tb, 0, INDEX_TB_L * sizeof(qoi_rgb_t));
	px = (qoi_rgb_t){ 0, 0, 0 };

	qoi_rgb_t predict;
	int predict_err[3] = { 0, 0, 0 };

	init_predict_iter(img_w, &predict);

	int p = 0;
	unsigned char run = 0;

	for (int px_pos = 0; px_pos < px_len; px_pos += 3) {
		if (run > 0) {
			run--;
		}
		else {
			unsigned char b1 = pencoded[p++];

			//110XXXXX DIFF2
			if ((b1 & QOI_MASK_3) == QOI_OP_DIFF2) {
				unsigned char b2 = pencoded[p++];
				unsigned char b3 = pencoded[p++];

				unsigned char vr = (b1 & 0x1f);
				unsigned char vg = ((b2 & 0xfc) >> 2);
				unsigned char vb = ((b3 & 0xfe) >> 1);

				vr |= ((b2 & 0x03) << 5);
				vg |= (b3 & 0x01) << 6;

				// ����λ��չ
				if (vr & 0x40) {
					vr |= 0x80;
				}
				if (vg & 0x40) {
					vg |= 0x80;
				}
				if (vb & 0x40) {
					vb |= 0x80;
				}

				px.r = vr;
				px.g = vg;
				px.b = vb;
			}
			// 11111111 RGB
			else if (b1 == QOI_OP_RGB) {
				px = (qoi_rgb_t){ pencoded[p], pencoded[p + 1], pencoded[p + 2] };
				p += 3;
			}
			// 111XXXXX�Ҳ���11111111 RUN
			else if ((b1 & QOI_MASK_3) == QOI_OP_RUN) {
				run = (b1 & 0x1f);
			}
			//000XXXXX INDEX
			else if ((b1 & QOI_MASK_3) == QOI_OP_INDEX) {
				px = index_tb[b1 % INDEX_TB_L];
			}
			//001XXXXX DIFF3
			else if ((b1 & QOI_MASK_3) == QOI_OP_DIFF3) {
				unsigned char b2 = pencoded[p++];

				unsigned char vr = (b2 >> 4);
				unsigned char vg = b1 & 0x1f;
				unsigned char vb = b2 & 0x0f;

				// ����λ��չ
				if (vr & 0x08) {
					vr |= 0xf0;
				}
				if (vg & 0x10) {
					vg |= 0xe0;
				}
				if (vb & 0x08) {
					vb |= 0xf0;
				}

				px.r = vr;
				px.g = vg;
				px.b = vb;
			}
			//01XXXXXX DIFF
			else if ((b1 & QOI_MASK_2) == QOI_OP_DIFF) {
				unsigned char vr = (b1 >> 4) & 0x03;
				unsigned char vg = (b1 >> 2) & 0x03;
				unsigned char vb = b1 & 0x03;

				// ����λ��չ
				if (vr & 0x02) {
					vr |= 0xfc;
				}
				if (vg & 0x02) {
					vg |= 0xfc;
				}
				if (vb & 0x02) {
					vb |= 0xfc;
				}

				px.r = vr;
				px.g = vg;
				px.b = vb;
			}
			//10XXXXXX LUMA
			else if ((b1 & QOI_MASK_2) == QOI_OP_LUMA) {
				unsigned char b2 = pencoded[p++];

				unsigned char vg = b1 & 0x3f;
				unsigned char vg_r = (b2 >> 4) & 0x0f;
				unsigned char vg_b = b2 & 0x0f;

				// ����λ��չ
				if (vg & 0x20) {
					vg |= 0xc0;
				}
				if (vg_r & 0x08) {
					vg_r |= 0xf0;
				}
				if (vg_b & 0x08) {
					vg_b |= 0xf0;
				}

				px.r = vg + vg_r;
				px.g = vg;
				px.b = vg + vg_b;
			}

			if ((b1 & QOI_MASK_2) == QOI_OP_DIFF || (b1 & QOI_MASK_2) == QOI_OP_LUMA || (b1 & QOI_MASK_3) == QOI_OP_DIFF2 ||
				(b1 & QOI_MASK_3) == QOI_OP_DIFF3) {
				px.r += predict.r;
				px.g += predict.g;
				px.b += predict.b;
			}

			index_tb[QOI_COLOR_HASH(px) % INDEX_TB_L] = px;
		}

		pdecoded[px_pos] = px.b;
		pdecoded[px_pos + 1] = px.g;
		pdecoded[px_pos + 2] = px.r;

		// ����Ԥ�����
		predict_err[0] = px.r - predict.r;
		predict_err[1] = px.g - predict.g;
		predict_err[2] = px.b - predict.b;

		get_next_predict_v(&px, &predict, predict_err);
	}

	clear_predict_iter();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*************************
@init
@private
@brief  ��ʼ��Ԥ�������
@param  w ���
		chn ͨ����
		predict ���ڴ�ŵ�ǰԤ��ֵ�����ؽṹ��(ָ��)
@return �Ƿ�ɹ�
*************************/
_Bool init_predict_iter(int w, qoi_rgb_t* predict) {
	predict_decode_first_line = 1;
	rgb_pre_line = malloc(sizeof(qoi_rgb_t) * w);
	predict_w = w;
	predict_column_i = 0;

	predict->r = 0;
	predict->g = 0;
	predict->b = 0;

	return rgb_pre_line != NULL;
}

/*************************
@delete
@private
@brief  ����Ԥ�������
@param  none
@return none
*************************/
void clear_predict_iter(void) {
	if (rgb_pre_line != NULL) {
		free(rgb_pre_line);
	}
}

/*************************
@decoder
@private
@brief  ��ȡ��һ��Ԥ��ֵ
@param  rgb ��ǰ����ֵ(ָ��)
		predict ��ǰԤ��ֵ(ָ��)
		predict_err Ԥ���������(�׵�ַ)
@return none
*************************/
void get_next_predict_v(qoi_rgb_t* rgb, qoi_rgb_t* predict, int* predict_err) {
	// �������ػ�����
	if (predict_column_i) {
		rgb_pre_line[predict_column_i - 1] = rgb_pre;
	}
	if (predict_column_i == predict_w - 1) {
		rgb_pre_line[predict_column_i] = *rgb;
	}
	rgb_pre = *rgb;
	if (predict_column_i == predict_w - 1) {
		predict_column_i = 0;
		predict_decode_first_line = 0;
	}
	else {
		predict_column_i++;
	}

	// ����Ԥ��ֵ
	if (predict_decode_first_line) {
		// ��ǰ:��1�е�(2+)��
		*predict = rgb_pre;
	}
	else {
		if (predict_column_i) {
			// ��ǰ:��(2+)�е�(2+)��
			qoi_rgb_t a_rgb = rgb_pre;
			qoi_rgb_t b_rgb = rgb_pre_line[predict_column_i];
			qoi_rgb_t c_rgb = rgb_pre_line[predict_column_i - 1];

			unsigned char* a = &(a_rgb.r);
			unsigned char* b = &(b_rgb.r);
			unsigned char* c = &(c_rgb.r);
			unsigned char* res = &(predict->r);

			for (unsigned char i = 0; i < 3; i++) {
				if (c[i] >= __MAX(a[i], b[i])) {
					res[i] = __MIN(a[i], b[i]);
				}
				else if (c[i] <= __MIN(a[i], b[i])) {
					res[i] = __MAX(a[i], b[i]);
				}
				else {
					res[i] = a[i] + b[i] - c[i];
				}
			}
		}
		else {
			// ��ǰ:��(2+)�е�1��
			*predict = rgb_pre_line[0];
		}
	}
}
