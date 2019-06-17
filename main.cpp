#include <stdio.h>
#include <thread>
#include <stdlib.h>
#include <iostream>

using namespace std;

//��������� BITMAPFILEHEADER � windows.h
#pragma pack(push,2)
struct BFH {
	unsigned short	bfType;
	unsigned long	bfSize;
	unsigned short	bfReserved1;
	unsigned short	bfReserved2;
	unsigned long	bfOffBits;
};
#pragma pack(pop)

//��������� BITMAPINFOHEADER � windows.h
struct BIH {
	unsigned long	biSize;
	long	biWidth;
	long	biHeight;
	unsigned short	biPlanes;
	unsigned short	biBitCount;
	unsigned long	biCompression;
	unsigned long	biSizeImage;
	long	biXPelsPerMeter;
	long	biYPelsPerMeter;
	unsigned long	biClrUsed;
	unsigned long	biClrImportant;
};

//��������� RGB ��������
struct RGB {
	unsigned char rgbtBlue;
	unsigned char rgbtGreen;
	unsigned char rgbtRed;
};

//������� �������������� RGB ������������ � yuv444 � � yuv444p.
void RGBto444p(int height, int width, int in_width, struct RGB rgb_l, FILE *f1, int len, unsigned char *YUV444, unsigned char *YUV444p, unsigned char *YUV420p){
    // ���������� RGB ������������ � ������ � yuv444 � � yuv444p
	for(int j = 0; j < height; j++) {
		for(int i = 0; i < width; i++) {

            fread(&rgb_l, sizeof(rgb_l), 1, f1); //������ �������� �� ��������� � RGB �������

			int Y = 0.299 * rgb_l.rgbtRed + 0.587 * rgb_l.rgbtGreen + 0.114 * rgb_l.rgbtBlue; //Y ������������
			int U = -0.169 * rgb_l.rgbtRed - 0.331 * rgb_l.rgbtGreen + 0.500 * rgb_l.rgbtBlue + 128; //U ������������
			int V = 0.500 * rgb_l.rgbtRed - 0.419 * rgb_l.rgbtGreen - 0.081 * rgb_l.rgbtBlue + 128; //V ������������

			// �������� �������� ������ ���� [0,255]
			if (Y < 0) Y = 0;
			else if (Y > 255) Y = 255;
			if (U < 0) U = 0;
			else if (U > 255) U = 255;
			if (V < 0) V = 0;
			else if (V > 255) V = 255;

			YUV444[(i + j * width) * 3] = Y;
			YUV444[(i + j * width) * 3 + 1] = U;
			YUV444[(i + j * width) * 3 + 2] = V;

			YUV444p[i + j * width] = Y;
			YUV444p[i + j * width + len] = U;
			YUV444p[i + j * width + 2*len] = V;

            //������ Y c����������� � yuv444p � yuv420p
			YUV420p[i + j * width] = Y;
		}
			if (!(in_width%4 == 0)){
            int A[in_width%4]; //������ ���������� ������ ������ � ����� ������
            fread(A, sizeof(A), 1, f1); //������ ������ ������ � ����� ������
        }
	}
}

//������� �������������� yuv444p � yuv420p.
void yuv444pto420p(int height, int width, int len, unsigned char *YUV444p, unsigned char *YUV420p){
    // ������ U � V ������������ �� yuv444p � yuv420p
    int k = 0;
    for(int j = 0; j < height; j += 2)
        {
            for(int i = 0; i < width; i += 2)
            {
                // ������ U ������������ ��� 420p �� 444p
                int sum = YUV444p[i + 0 + j*width + len] ; // ������� 1.1
                sum += YUV444p[i + 1 + j*width + len]; //������� 1.2
                sum += YUV444p[i + 0 + (j+1)*width + len]; //������� 2.1
                sum += YUV444p[i + 1 + (j+1)*width + len]; //������� 2.2

                sum = sum / 4 ; //������� 4 U ������������ � ��������� �������� ��������

                YUV420p[k + len] = sum ; //������ U ������������ � 420p

                sum = YUV444p[i + 0 + j*width + 2*len] ; // ������� 1.1
                sum += YUV444p[i + 1 + j*width + 2*len]; //������� 1.2
                sum += YUV444p[i + 0 + (j+1)*width + 2*len]; //������� 2.1
                sum += YUV444p[i + 1 + (j+1)*width + 2*len]; //������� 2.2

                sum = sum / 4 ; //������� 4 V ������������ � ��������� �������� ��������

                YUV420p[k + len*5/4] = sum ; //������ V ������������ � 420p

                k++; //������ ������ �� ��������� � 1 �����
            }
        }
}

//������� ��������� ��������������� �������� � ��������
void BMPtoYUV(int height, int width, int len, int yuv_width, int yuv_len, int yuv_fr_count,  FILE *f0,  FILE *f2,  FILE *f3, unsigned char *VIDEO_frame, unsigned char *YUV420p){

    // ������ ������ � 420p. ���������� ������ Y[len],U[len/4],V(len/4)
	for(int j = 0; j < len*3/2; j++)
	    fwrite(&YUV420p[j], sizeof(unsigned char),1, f2);

    // ������ �������� � �������� ���������
	for(int fr_num = 0; fr_num < yuv_fr_count; fr_num++)
	{
		fseek(f0, yuv_len * fr_num * 3/2, 0); //����� ������� � ����� �������� ��������� �� �������� ����
		fread(VIDEO_frame, 1, yuv_len * 3/2, f0);  //������ ����� �� ����� �������� ���������

		for(int j = 0; j < height; j++)
		{
			int S1 = yuv_width * j; //�������� �� ����� ������ ����� � ���������
			int S2 = width * j; //�������� �� ����� ������ ��������

			for(int i = 0; i < width; i++)
				VIDEO_frame[i + S1] = YUV420p[i + S2]; //������ Y ������������ �������� � ����
		}

		for(int j = 0; j < height / 2; j++)
		{
			int S1 = yuv_width / 2 * j; //�������� �� ����� ������ ����� � ���������
			int S2 = width / 2 * j; //�������� �� ����� ������ ��������

			for(int i = 0; i < (width / 2); i++)
			{
				VIDEO_frame[i + yuv_len + S1] = YUV420p[i + len + S2]; //������ U ������������ �������� � ����
                VIDEO_frame[i + S1 + yuv_len * 5 / 4 ] = YUV420p[i + S2 + len * 5 / 4]; //������ V ������������ �������� � ����
			}
		}

	    fwrite(VIDEO_frame, 1, yuv_len * 3/2, f3); //������ ����� � ���� ��������� ���������
	}
}

int main()
{

    int in_width = 74; //������ �������� � ��������
    int height = 50; //������ �������� � ��������

    int width;
    !(in_width%4 == 0) ? width = in_width - in_width%4 : width = in_width; //�������, ���� � �������� ������� ������ ����� � ����� ������

    int len = width * height; //���������� �������� � ��������
    int yuv_width = 176; //������ ��������� � ��������
    int yuv_height = 144; //������ ��������� � ��������
    int yuv_len = yuv_width*yuv_height; //���������� �������� � ���������
    int yuv_fr_count = 150; //����� ������ �������� ���������

    // ��������� ��������� BMP-�����
    BFH  bfh_l;
    BIH bih_l;
    RGB rgb_l;

    // ��������� ��������� �� �����
    FILE  * f0, * f1, * f2, * f3;

    f0 = fopen("suzie_yuv420.yuv", "r+b");
    f1 = fopen("RGB1.bmp", "r+b");
    f2 = fopen("yuv420p.yuv", "w+b");
    f3 = fopen("out.yuv", "w+b");

	// ��������� �� ������ � ���������� ���������� �������
    unsigned char *YUV444;
    unsigned char *YUV444p;
    unsigned char *YUV420p;
    unsigned char *VIDEO_frame;

    YUV444 = (unsigned char *) calloc(len*3, 1);
    YUV444p = (unsigned char *) calloc(len*3, 1);
    YUV420p = (unsigned char *) calloc(len*3/2, 1);
    VIDEO_frame = (unsigned char *) calloc(yuv_len*3/2, 1);

    // ���������� ����� bmp �����
    fread(&bfh_l,sizeof(bfh_l),1,f1);
    fread(&bih_l,sizeof(bih_l),1,f1);

    std::thread thr(RGBto444p, height, width, in_width, rgb_l, f1, len, YUV444, YUV444p, YUV420p);
    if (thr.joinable())
        thr.join();
    std::thread thr2(yuv444pto420p, height, width, len, YUV444p, YUV420p);
    if (thr2.joinable())
        thr2.join();
    std::thread thr3(BMPtoYUV, height, width, len, yuv_width, yuv_len, yuv_fr_count, f0, f2, f3, VIDEO_frame, YUV420p);
    if (thr3.joinable())
        thr3.join();

    //--- ������������ ������
    free(YUV444);
    free(YUV444p);
    free(YUV420p);
    free(VIDEO_frame);
    //---

   return 0;
}
