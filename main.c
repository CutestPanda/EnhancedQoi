#include "enhanced_qoi.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
	unsigned short width;
	unsigned short height;
	unsigned int encoded_len;
}QoiHeader;

////////////////////////////////////////////////////////////////////////////////////////////////////////////

int test_encoder(const char* rgb_img_path, const char* encoded_bin_path);
int test_decoder(const char* encoded_bin_path, const char* rgb_img_path);
int compare_bmp(char* file1, char* file2);

////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main() {
	// return test_encoder("test/in7.bmp", "test/compressed.bin");
	// return test_decoder("test/compressed.bin", "test/out.bmp");
	// return compare_bmp("test/in7.bmp", "test/out.bmp");
}

int test_encoder(const char* rgb_img_path, const char* encoded_bin_path) {
	int width, height, nrChannels;

	unsigned char* data = stbi_load(rgb_img_path, &width, &height, &nrChannels, STBI_rgb);
	unsigned char* compressed = malloc(width * height * 4);

	printf(" ‰»ÎÕº∆¨(w%d h%d)\n", width, height);
	
	if (compressed == NULL || data == NULL) {
		return -1;
	}

	int compressed_len = enhanced_qoi_encode(data, compressed, width, height);

	printf("—πÀı¬  = %f\n", compressed_len * 1.0f / (width * height * 3));

	FILE* file;

	fopen_s(&file, encoded_bin_path, "wb");

	if (file == NULL) {
		return -1;
	}

	QoiHeader header;
	header.width = (unsigned short)width;
	header.height = (unsigned short)height;
	header.encoded_len = (unsigned int)compressed_len;

	fwrite(&header, sizeof(QoiHeader), 1, file);
	fwrite(compressed, 1, compressed_len, file);

	fclose(file);

	stbi_image_free(data);
	free(compressed);

	return 0;
}

int test_decoder(const char* encoded_bin_path, const char* rgb_img_path) {
	FILE* file;

	fopen_s(&file, encoded_bin_path, "rb");

	if (file == NULL) {
		return -1;
	}

	QoiHeader header;

	fread_s(&header, sizeof(QoiHeader), sizeof(QoiHeader), 1, file);

	unsigned char* data = malloc(header.width * header.height * 3);
	unsigned char* compressed = malloc(header.encoded_len);

	if (compressed == NULL || data == NULL) {
		return -1;
	}

	fread_s(compressed, header.encoded_len, 1, header.encoded_len, file);

	enhanced_qoi_decode(compressed, data, header.width, header.height);

	stbi_write_bmp(rgb_img_path, header.width, header.height, STBI_rgb, data);

	printf(" ‰≥ˆÕº∆¨(w%d h%d)\n", header.width, header.height);

	fclose(file);

	free(data);
	free(compressed);

	return 0;
}

int compare_bmp(char* file1, char* file2) {
	int width, height, nrChannels;
	int w2, h2, ch2;
	unsigned char* data1 = stbi_load(file1, &width, &height, &nrChannels, STBI_rgb_alpha);

	if (data1 == NULL) {
		printf("ERROR: cannot open file1: %s\n", file1);

		return -1;
	}

	unsigned char* data2 = stbi_load(file2, &w2, &h2, &ch2, STBI_rgb_alpha);
	if (NULL == data2) {
		stbi_image_free(data1);
		printf("ERROR: cannot open file2: %s\n", file2);

		return -1;
	}
	printf("INFO: file1 size = %dx%d (%d channels)\n", width, height, nrChannels);
	printf("INFO: file2 size = %dx%d (%d channels)\n", w2, h2, ch2);

	if (width != w2 || height != h2 || nrChannels != ch2) {
		printf("INFO: the two files's size not equal\n");
	}
	else {
		unsigned int* pImg1 = (unsigned int*)data1;
		unsigned int* pImg2 = (unsigned int*)data2;
		int errCnt = 0;
		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				if (*pImg1 != *pImg2) {
					errCnt++;
				}
				pImg1++;
				pImg2++;
			}
		}
		if (errCnt > 0) {
			printf("INFO: there are %d different pixels\n", errCnt);

			return -1;
		}
		else {
			printf("INFO: the two files are same\n");
		}
	}

	stbi_image_free(data1);
	stbi_image_free(data2);

	return 0;
}
