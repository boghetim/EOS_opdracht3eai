/*
 * vdmaTest.c
 *
 *  Created on: Apr 9, 2020
 *      Author: VIPIN
 */
#include "xparameters.h"
#include "xaxivdma.h"
#include "xscugic.h"
#include "sleep.h"
#include <stdlib.h>
#include "xil_cache.h"
#include "xil_cache.h"

#define HSize 1920
#define VSize 1080
#define FrameSize HSize*VSize*3

const int vierOpEenRij = 4;
const int CellWidth = HSize / vierOpEenRij;
const int CellHeight = VSize / vierOpEenRij;

static XScuGic Intc;

static int SetupIntrSystem(XAxiVdma *AxiVdmaPtr, u16 ReadIntrId);
void drawGrid(unsigned char* buffer);
void updateStartPosition();
void drawToken(unsigned char* buffer);

unsigned char Buffer[FrameSize];

int CheckRooster[4] = {0, 0, 0, 0};

int positionPlayer1 = 0;
int positionPlayer2 = 0;

int main(){
	int status;
	int Index;
	u32 Addr;
	XAxiVdma myVDMA;
	XAxiVdma_Config *config = XAxiVdma_LookupConfig(XPAR_AXI_VDMA_0_DEVICE_ID);
	XAxiVdma_DmaSetup ReadCfg;
	status = XAxiVdma_CfgInitialize(&myVDMA, config, config->BaseAddress);
    if(status != XST_SUCCESS){
    	xil_printf("DMA Initialization failed");
    }
    ReadCfg.VertSizeInput = VSize;
    ReadCfg.HoriSizeInput = HSize*3;
    ReadCfg.Stride = HSize*3;
    ReadCfg.FrameDelay = 0;
    ReadCfg.EnableCircularBuf = 1;
    ReadCfg.EnableSync = 1;
    ReadCfg.PointNum = 0;
    ReadCfg.EnableFrameCounter = 0;
    ReadCfg.FixedFrameStoreAddr = 0;
    status = XAxiVdma_DmaConfig(&myVDMA, XAXIVDMA_READ, &ReadCfg);
    if (status != XST_SUCCESS) {
    	xil_printf("Write channel config failed %d\r\n", status);
    	return status;
    }

    Addr = (u32)&(Buffer[0]);


	for(Index = 0; Index < myVDMA.MaxNumFrames; Index++) {
		ReadCfg.FrameStoreStartAddr[Index] = Addr;
		Addr +=  FrameSize;
	}

	status = XAxiVdma_DmaSetBufferAddr(&myVDMA, XAXIVDMA_READ,ReadCfg.FrameStoreStartAddr);
	if (status != XST_SUCCESS) {
		xil_printf("Read channel set buffer address failed %d\r\n", status);
		return XST_FAILURE;
	}

	XAxiVdma_IntrEnable(&myVDMA, XAXIVDMA_IXR_COMPLETION_MASK, XAXIVDMA_READ);

	SetupIntrSystem(&myVDMA, XPAR_FABRIC_AXI_VDMA_0_MM2S_INTROUT_INTR);

	drawGrid(Buffer);
	xil_printf("rooster, check\r\n");
	positionPlayer1 = 0;
	positionPlayer2 = 1;

	Addr = (u32)&(Buffer[0]);

	drawToken(Buffer);
	xil_printf("1ste disks, check\r\n");
	positionPlayer1 = 1;
	positionPlayer2 = 2;

	Addr = (u32)&(Buffer[0]);

	drawToken(Buffer);
	xil_printf("2de disks, check\r\n");

	positionPlayer1 = 2;
	positionPlayer2 = 0;
	drawToken(Buffer);

	positionPlayer1 = 3;
	positionPlayer2 = 1;
	drawToken(Buffer);


	Xil_DCacheFlush();

	status = XAxiVdma_DmaStart(&myVDMA,XAXIVDMA_READ);
	if (status != XST_SUCCESS) {
		if(status == XST_VDMA_MISMATCH_ERROR)
			xil_printf("DMA Mismatch Error\r\n");
		return XST_FAILURE;
	}

    while(1){
    }
}


/*****************************************************************************/
 /* Call back function for read channel
******************************************************************************/

static void ReadCallBack(void *CallbackRef, u32 Mask)
{

	//static int i=0;
	/* User can add his code in this call back function */
	/*
	xil_printf("Read Call back function is called\r\n");
	if(i==0){
		memset(Buffer,0x00,FrameSize);
		i=1;
	}
	else{
		memset(Buffer,0xff,FrameSize);
		i=0;
	}
	Xil_DCacheFlush();
	sleep(1);
	*/
}

/*****************************************************************************/
/*
 * The user can put his code that should get executed when this
 * call back happens.
 *
*
******************************************************************************/
static void ReadErrorCallBack(void *CallbackRef, u32 Mask)
{
	/* User can add his code in this call back function */
	xil_printf("Read Call back Error function is called\r\n");

}


static int SetupIntrSystem(XAxiVdma *AxiVdmaPtr, u16 ReadIntrId)
{
	int Status;
	XScuGic *IntcInstancePtr =&Intc;

	/* Initialize the interrupt controller and connect the ISRs */
	XScuGic_Config *IntcConfig;
	IntcConfig = XScuGic_LookupConfig(XPAR_PS7_SCUGIC_0_DEVICE_ID);
	Status =  XScuGic_CfgInitialize(IntcInstancePtr, IntcConfig, IntcConfig->CpuBaseAddress);
	if(Status != XST_SUCCESS){
		xil_printf("Interrupt controller initialization failed..");
		return -1;
	}

	Status = XScuGic_Connect(IntcInstancePtr,ReadIntrId,(Xil_InterruptHandler)XAxiVdma_ReadIntrHandler,(void *)AxiVdmaPtr);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed read channel connect intc %d\r\n", Status);
		return XST_FAILURE;
	}

	XScuGic_Enable(IntcInstancePtr,ReadIntrId);

	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,(Xil_ExceptionHandler)XScuGic_InterruptHandler,(void *)IntcInstancePtr);
	Xil_ExceptionEnable();

	/* Register call-back functions
	 */
	XAxiVdma_SetCallBack(AxiVdmaPtr, XAXIVDMA_HANDLER_GENERAL, ReadCallBack, (void *)AxiVdmaPtr, XAXIVDMA_READ);

	XAxiVdma_SetCallBack(AxiVdmaPtr, XAXIVDMA_HANDLER_ERROR, ReadErrorCallBack, (void *)AxiVdmaPtr, XAXIVDMA_READ);

	return XST_SUCCESS;
}

void drawGrid(unsigned char* buffer) {
    // Vul de buffer met wit
    for (int i = 0; i < VSize; ++i) {
        for (int j = 0; j < HSize * 3; j = j + 3) {
            buffer[(i * HSize * 3) + j] = 0xff;     // R
            buffer[(i * HSize * 3) + j + 1] = 0xff; // G
            buffer[(i * HSize * 3) + j + 2] = 0xff; // B
        }
    }

    // Teken horizontale lijnen
    for (int i = 1; i <= vierOpEenRij - 1; ++i) {
        int yPos = i * CellHeight - 5; // Aanpassen voor centrering van de lijn
        for (int j = 0; j < HSize * 3; j = j + 3) {
            buffer[(yPos * HSize * 3) + j] = 0x00;     // G (zwart)
            buffer[(yPos * HSize * 3) + j + 1] = 0x00; // B (zwart)
            buffer[(yPos * HSize * 3) + j + 2] = 0x00; // R (zwart)
        }
    }

    // Teken verticale lijnen
    for (int i = 1; i <= vierOpEenRij - 1; ++i) {
        int xPos = i * CellWidth - 5; // Aanpassen voor centrering van de lijn
        for (int j = 0; j < VSize; ++j) {
            buffer[(j * HSize * 3) + (xPos * 3)] = 0x00;     // G (zwart)
            buffer[(j * HSize * 3) + (xPos * 3) + 1] = 0x00; // B (zwart)
            buffer[(j * HSize * 3) + (xPos * 3) + 2] = 0x00; // R (zwart)
        }
    }

}

void drawToken(unsigned char* buffer) {

	updateStartPosition();

    // Teken de token voor op de positie van speler 1
    int xStartX = (positionPlayer1 % vierOpEenRij) * CellWidth;
    int yStartX = (positionPlayer1 / vierOpEenRij) * CellHeight;
    int halfCellWidthX = CellWidth / 2;
    int halfCellHeightX = CellHeight / 2;
    int startX_X = xStartX + (CellWidth - halfCellWidthX) / 2;
    int startY_X = yStartX + (CellHeight - halfCellHeightX) / 2;
    for (int i = 0; i < halfCellHeightX; ++i) {
        for (int j = 0; j < halfCellWidthX; ++j) {
            int bufferIndex = ((startY_X + i) * HSize + (startX_X + j)) * 3;
            buffer[bufferIndex] = 0xff;     // G (zwart)
            buffer[bufferIndex + 1] = 0x00; // B (zwart)
            buffer[bufferIndex + 2] = 0xff; // R (zwart)
        }
    }

    // Teken de token voor op de positie van speler 1
    int xStartO = (positionPlayer2 % vierOpEenRij) * CellWidth;
    int yStartO = (positionPlayer2 / vierOpEenRij) * CellHeight;
    int halfCellWidthO = CellWidth / 2;
    int halfCellHeightO = CellHeight / 2;
    int startX_O = xStartO + (CellWidth - halfCellWidthO) / 2;
    int startY_O = yStartO + (CellHeight - halfCellHeightO) / 2;
    for (int i = 0; i < halfCellHeightO; ++i) {
        for (int j = 0; j < halfCellWidthO; ++j) {
            int bufferIndex = ((startY_O + i) * HSize + (startX_O + j)) * 3;
            buffer[bufferIndex] = 0x00;     // G (zwart)
            buffer[bufferIndex + 1] = 0x00; // B (zwart)
            buffer[bufferIndex + 2] = 0xff; // R (zwart)
        }
    }

}

void updateStartPosition() {
    if (positionPlayer1 >= 0 && positionPlayer1 < 4) {
        if (CheckRooster[positionPlayer1] == 0) {
        	CheckRooster[positionPlayer1] += 1;
        	positionPlayer1 +=12;
        }
        else if (CheckRooster[positionPlayer1] == 1) {
        	CheckRooster[positionPlayer1] += 1;
        	positionPlayer1 +=8;
        }
        else if (CheckRooster[positionPlayer1] == 2) {
        	CheckRooster[positionPlayer1] += 1;
        	positionPlayer1 +=4;
        }
        else if (CheckRooster[positionPlayer1] == 3) {
        	CheckRooster[positionPlayer1] += 1;
        	//waarde blijft 0
        }
        else{
        	xil_printf("value must be 0 till 3, value is now:%d\r\n", positionPlayer1);
        }

        xil_printf("player1:%d\r\n", positionPlayer1);

    }

    if (positionPlayer2 >= 0 && positionPlayer2 < 4) {
        if (CheckRooster[positionPlayer2] == 0) {
        	CheckRooster[positionPlayer2] += 1;
        	positionPlayer2 +=12;
        }
        else if (CheckRooster[positionPlayer2] == 1) {
        	CheckRooster[positionPlayer2] += 1;
        	positionPlayer2 +=8;
        }
        else if (CheckRooster[positionPlayer2] == 2) {
        	CheckRooster[positionPlayer2] += 1;
        	positionPlayer2 +=4;
        }
        else if (CheckRooster[positionPlayer2] == 3) {
        	CheckRooster[positionPlayer2] += 1;
        	//waarde blijft 0
        }
        else{
        	xil_printf("value must be 0 till 3, value is now:%d\r\n", positionPlayer2);
        }

        xil_printf("player2:%d\r\n", positionPlayer2);
        xil_printf("array is :%d\r\n", CheckRooster[0]);
        xil_printf("array is :%d\r\n", CheckRooster[1]);
        xil_printf("array is :%d\r\n", CheckRooster[2]);
        xil_printf("array is :%d\r\n", CheckRooster[3]);
    }

}