// simpleTest.cpp : �������̨Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>
#include <GL/glut.h>
#include <AR/gsub.h>
#include <AR/video.h>
#include <AR/param.h>
#include <AR/ar.h>
//�����Ĭ�ϲ���
char			*vconf = "Data\\WDM_camera_flipV.xml";
//�������������
char           *cparam_name = "Data/camera_para.dat";
//��ʶ��Ϣ
char           *patt_name = "Data/patt.hiro";

int             xsize, ysize;
int             thresh = 100;
int             count = 0;
ARParam         cparam;
int             patt_id;
double          patt_width = 80.0;
double          patt_center[2] = { 0.0, 0.0 };
double          patt_trans[3][4];

static void   init(void);
static void   cleanup(void);
static void   keyEvent(unsigned char key, int x, int y);
static void   mainLoop(void);
static void   draw(void);

int main(int argc, char **argv)
{
	//opengl��ʼ��
	glutInit(&argc, argv);
	//��ʼ����β��� ����ʾ����
	init();
	//�������
	arVideoCapStart();
	//����֡ѭ�������趨��Ӧ�¼���������Ϊ����ָ�룩
	//argMainLoop()���� ���������� �ֱ��� mouseEvent keyEvent mainLoop
	argMainLoop(NULL, keyEvent, mainLoop);
	return (0);
}

static void   keyEvent(unsigned char key, int x, int y)
{
	if (key == 0x1b) {
		printf("*** %f (frame/sec)\n", (double)count / arUtilTimer());
		cleanup();
		exit(0);
	}
}

/* main loop */
static void mainLoop(void)
{
	ARUint8         *dataPtr;
	ARMarkerInfo    *marker_info; //��־��Ϣstruct
	int             marker_num;
	int             j, k;

	//��ȡһ֡ͼ��
	if ((dataPtr = (ARUint8 *)arVideoGetImage()) == NULL) {
		//�����ӳ�
		arUtilSleep(2);
		return;
	}
	if (count == 0) arUtilTimerReset();
	count++;
	//Ϊ����Ⱦ2d ���µ�ǰ�������
	argDrawMode2D();
	argDispImage(dataPtr, 0, 0);
	//����ʶ
	/*
	arDetectMarker()�Ĳ����ֱ���
	dataPtr ֡����
	thresh  ��ֵ����ֵ
	marker_info  ��ʶ������Ϣ
	marker_num ��ʶ����
	*/
	if (arDetectMarker(dataPtr, thresh, &marker_info, &marker_num) < 0) {
		cleanup();
		exit(0);
	}
	//ÿһ֡��Ҫ���ã�֧����๦�ܵ����
	arVideoCapNext();

	k = -1;
	for (j = 0; j < marker_num; j++) {
		if (patt_id == marker_info[j].id) {
			if (k == -1) k = j;
			else if (marker_info[k].cf < marker_info[j].cf) k = j;
		}
	}
	if (k == -1) {
		argSwapBuffers();
		return;
	}
	//��ȡ�����λ��
	arGetTransMat(&marker_info[k], patt_center, patt_width, patt_trans);
	//����ģ�͵���Ӧ��λ��
	draw();

	argSwapBuffers();
}

static void init(void)
{
	ARParam  wparam;

	//����������ļ�
	arVideoOpen(vconf);
	//��ȡ��Ƶ���ڴ�С
	arVideoInqSize(&xsize, &ysize);
	//���������������
	arParamLoad(cparam_name, 1, &wparam);
	arParamChangeSize(&wparam, xsize, ysize, &cparam);
	//��ʼ�����
	arInitCparam(&cparam);
	arParamDisp(&cparam);
	//��ȡ�����ʶ �Ķ����ļ�
	patt_id = arLoadPatt(patt_name);
	//��ͼ�񴰿�
	argInit(&cparam, 1.0, 0, 0, 0, 0);
}

//cleanup
static void cleanup(void)
{
	arVideoCapStop();
	arVideoClose();
	argCleanup();
}
//����3Dģ�� ��opengl������
static void draw(void)
{
	double    gl_para[16];
	GLfloat   mat_ambient[] = { 0.0, 0.0, 1.0, 1.0 };
	GLfloat   mat_flash[] = { 0.0, 0.0, 1.0, 1.0 };
	GLfloat   mat_flash_shiny[] = { 50.0 };
	GLfloat   light_position[] = { 100.0,-200.0,200.0,0.0 };
	GLfloat   ambi[] = { 0.1, 0.1, 0.1, 0.1 };
	GLfloat   lightZeroColor[] = { 0.9, 0.9, 0.9, 0.1 };

	argDrawMode3D();
	argDraw3dCamera(0, 0);
	glClearDepth(1.0);
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	//�������ת������
	argConvGlpara(patt_trans, gl_para);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixd(gl_para);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_POSITION, light_position);
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambi);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, lightZeroColor);
	glMaterialfv(GL_FRONT, GL_SPECULAR, mat_flash);
	glMaterialfv(GL_FRONT, GL_SHININESS, mat_flash_shiny);
	glMaterialfv(GL_FRONT, GL_AMBIENT, mat_ambient);
	glMatrixMode(GL_MODELVIEW);
	glTranslatef(0.0, 0.0, 25.0);
	glutSolidCube(50.0);
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
}

