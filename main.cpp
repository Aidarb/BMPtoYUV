#include <stdio.h>
#include <thread>
#include <stdlib.h>
#include <iostream>

using namespace std;

//структура BITMAPFILEHEADER с windows.h
#pragma pack(push,2)
struct BFH {
	unsigned short	bfType;
	unsigned long	bfSize;
	unsigned short	bfReserved1;
	unsigned short	bfReserved2;
	unsigned long	bfOffBits;
};
#pragma pack(pop)

//структура BITMAPINFOHEADER с windows.h
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

//структура RGB триплета
struct RGB {
	unsigned char rgbtBlue;
	unsigned char rgbtGreen;
	unsigned char rgbtRed;
};

//функция преобразования RGB составляющих в yuv444 и в yuv444p.
void RGBto444p(int height, int width, int in_width, struct RGB rgb_l, FILE *f1, int len, unsigned char *YUV444, unsigned char *YUV444p, unsigned char *YUV420p){
    // Считывание RGB составляющих и запись в yuv444 и в yuv444p
	for(int j = 0; j < height; j++) {
		for(int i = 0; i < width; i++) {

            fread(&rgb_l, sizeof(rgb_l), 1, f1); //чтение картинки по пиксельно в RGB триплет

			int Y = 0.299 * rgb_l.rgbtRed + 0.587 * rgb_l.rgbtGreen + 0.114 * rgb_l.rgbtBlue; //Y составляющая
			int U = -0.169 * rgb_l.rgbtRed - 0.331 * rgb_l.rgbtGreen + 0.500 * rgb_l.rgbtBlue + 128; //U составляющая
			int V = 0.500 * rgb_l.rgbtRed - 0.419 * rgb_l.rgbtGreen - 0.081 * rgb_l.rgbtBlue + 128; //V составляющая

			// диапазон значений должен быть [0,255]
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

            //запись Y cоставляющую с yuv444p в yuv420p
			YUV420p[i + j * width] = Y;
		}
			if (!(in_width%4 == 0)){
            int A[in_width%4]; //массив количество пустых байтов в конце строки
            fread(A, sizeof(A), 1, f1); //чтение пустых байтов в конце строки
        }
	}
}

//функция преобразования yuv444p в yuv420p.
void yuv444pto420p(int height, int width, int len, unsigned char *YUV444p, unsigned char *YUV420p){
    // Запись U и V составляющих из yuv444p в yuv420p
    int k = 0;
    for(int j = 0; j < height; j += 2)
        {
            for(int i = 0; i < width; i += 2)
            {
                // Расчет U составляющей для 420p из 444p
                int sum = YUV444p[i + 0 + j*width + len] ; // элемент 1.1
                sum += YUV444p[i + 1 + j*width + len]; //элемент 1.2
                sum += YUV444p[i + 0 + (j+1)*width + len]; //элемент 2.1
                sum += YUV444p[i + 1 + (j+1)*width + len]; //элемент 2.2

                sum = sum / 4 ; //деление 4 U составляющей и получение среднего значения

                YUV420p[k + len] = sum ; //запись U составляющей в 420p

                sum = YUV444p[i + 0 + j*width + 2*len] ; // элемент 1.1
                sum += YUV444p[i + 1 + j*width + 2*len]; //элемент 1.2
                sum += YUV444p[i + 0 + (j+1)*width + 2*len]; //элемент 2.1
                sum += YUV444p[i + 1 + (j+1)*width + 2*len]; //элемент 2.2

                sum = sum / 4 ; //деление 4 V составляющей и получение среднего значения

                YUV420p[k + len*5/4] = sum ; //запись V составляющей в 420p

                k++; //запись данных со смещением в 1 адрес
            }
        }
}

//функция внедрения преобразованной картинки в видеоряд
void BMPtoYUV(int height, int width, int len, int yuv_width, int yuv_len, int yuv_fr_count,  FILE *f0,  FILE *f2,  FILE *f3, unsigned char *VIDEO_frame, unsigned char *YUV420p){

    // Запись данных в 420p. Количество данных Y[len],U[len/4],V(len/4)
	for(int j = 0; j < len*3/2; j++)
	    fwrite(&YUV420p[j], sizeof(unsigned char),1, f2);

    // Запись картинки в видеоряд покадрово
	for(int fr_num = 0; fr_num < yuv_fr_count; fr_num++)
	{
		fseek(f0, yuv_len * fr_num * 3/2, 0); //сдвиг курсора в файле входного видеоряда на заданный кадр
		fread(VIDEO_frame, 1, yuv_len * 3/2, f0);  //чтение кадра из файла входного видеоряда

		for(int j = 0; j < height; j++)
		{
			int S1 = yuv_width * j; //смещение на новую строку кадра в видеоряде
			int S2 = width * j; //смещение на новую строку картинки

			for(int i = 0; i < width; i++)
				VIDEO_frame[i + S1] = YUV420p[i + S2]; //запись Y составляющей картинки в кадр
		}

		for(int j = 0; j < height / 2; j++)
		{
			int S1 = yuv_width / 2 * j; //смещение на новую строку кадра в видеоряде
			int S2 = width / 2 * j; //смещение на новую строку картинки

			for(int i = 0; i < (width / 2); i++)
			{
				VIDEO_frame[i + yuv_len + S1] = YUV420p[i + len + S2]; //запись U составляющей картинки в кадр
                VIDEO_frame[i + S1 + yuv_len * 5 / 4 ] = YUV420p[i + S2 + len * 5 / 4]; //запись V составляющей картинки в кадр
			}
		}

	    fwrite(VIDEO_frame, 1, yuv_len * 3/2, f3); //запись кадра в файл выходного видеоряда
	}
}

int main()
{

    int in_width = 74; //ширина картинки в пикселах
    int height = 50; //высота картинки в пикселах

    int width;
    !(in_width%4 == 0) ? width = in_width - in_width%4 : width = in_width; //условие, если в картинке имеются пустые байты в конце строки

    int len = width * height; //количество пикселей в картинке
    int yuv_width = 176; //ширина видеоряда в пикселах
    int yuv_height = 144; //высота видеоряда в пикселах
    int yuv_len = yuv_width*yuv_height; //количество пикселей в видеоряде
    int yuv_fr_count = 150; //число кадров входного видеоряда

    // Объявляем структуры BMP-файла
    BFH  bfh_l;
    BIH bih_l;
    RGB rgb_l;

    // Обьявляем указатели на файлы
    FILE  * f0, * f1, * f2, * f3;

    f0 = fopen("suzie_yuv420.yuv", "r+b");
    f1 = fopen("RGB1.bmp", "r+b");
    f2 = fopen("yuv420p.yuv", "w+b");
    f3 = fopen("out.yuv", "w+b");

	// Указатели на массив с опреденной выделенной памятью
    unsigned char *YUV444;
    unsigned char *YUV444p;
    unsigned char *YUV420p;
    unsigned char *VIDEO_frame;

    YUV444 = (unsigned char *) calloc(len*3, 1);
    YUV444p = (unsigned char *) calloc(len*3, 1);
    YUV420p = (unsigned char *) calloc(len*3/2, 1);
    VIDEO_frame = (unsigned char *) calloc(yuv_len*3/2, 1);

    // Считывание шапки bmp файла
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

    //--- Освобождение пямяти
    free(YUV444);
    free(YUV444p);
    free(YUV420p);
    free(VIDEO_frame);
    //---

   return 0;
}
