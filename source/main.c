#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <3ds.h>

#define SYSFONT_OUT_DIR  "sdmc:/3ds"
#define SYSFONT_OUT_PATH SYSFONT_OUT_DIR "/sysfont.bcfnt"

u8 patchError = 0;
u8 succeeded = 0;

void unpatchPtr(void** ptr, u32 base, u32 size)
{
	if (*ptr)
	{
		u32 addr = (u32)*ptr;
		if (addr >= base && addr < base + size)
		{
			*ptr = (void*)(addr - base);
		}
		else
		{
		    printf("Patch Error: %08lx (b=%08lx,s=%08lx)\n", addr, base, size);
		    patchError = 1;
			*ptr = NULL; // Should not happen with valid font
		}
	}
}

typedef struct {
	uint32_t status;
	uint32_t region;
	uint32_t size;
	uint8_t padding[0x74];
} RawFontHeader;

void dumpSystemFont()
{
	Result res = fontEnsureMapped();
	if (R_FAILED(res))
	{
		printf("fontEnsureMapped failed: %08lx\n", res);
		return;
	}

	CFNT_s* font = fontGetSystemFont();
	if (!font)
	{
		printf("fontGetSystemFont failed\n");
		return;
	}

	RawFontHeader *rawHeader = (RawFontHeader*)((u8*)font - 0x80);
	printf("Status: ");
	switch (rawHeader->status) {
        case 1:
            puts("Loading");
            break;
        case 2:
			puts("Loaded");
			break;
        case 3:
           	puts("Failed");
           	break;
		default:
		    puts("Invalid");
			return;
	}

	printf("Header Size: %lu bytes\n", rawHeader->size);

	printf("Region: ");
	switch (rawHeader->region) {
        case 1:
            puts("JPN/EUR/USA");
            break;
        case 2:
			puts("CHN");
			break;
		case 3:
           	puts("KOR");
           	break;
        case 4:
           	puts("TWN");
           	break;
		default:
		    puts("Invalid");
			return;
	}

	printf("Font Signature: %08lx\n", font->signature);
	if (font->signature != 0x554E4643) // 'CFNU'
	{
		printf("Unexpected font signature!\n");
		return;
	}

	u32 size = font->fileSize;
	printf("Font Size: %lu bytes\n", size);
	if (size < 0x100 || size > 0x1000000)
	{
		printf("Invalid font size!\n");
		return;
	}

	if (size != rawHeader->size)
	{
	    printf("Warning: mismatch on size from outer and inner header.\n");
	}

	void* buffer = malloc(size);
	if (!buffer)
	{
		printf("Failed to allocate buffer!\n");
		return;
	}

	memcpy(buffer, font, size);
	patchError = 0;
	u32 base = (u32)font;

	CFNT_s* bFont = (CFNT_s*)buffer;
	bFont->signature = 0x544E4643; // 'CFNT'

	FINF_s* mFinf = &font->finf;
	FINF_s* bFinf = &bFont->finf;

	unpatchPtr((void**)&bFinf->tglp, base, size);
	unpatchPtr((void**)&bFinf->cwdh, base, size);
	unpatchPtr((void**)&bFinf->cmap, base, size);

	if (mFinf->tglp)
	{
		TGLP_s* bTglp = (TGLP_s*)((u32)buffer + (u32)bFinf->tglp);
		unpatchPtr((void**)&bTglp->sheetData, base, size);
	}

	CWDH_s* mCwdh = mFinf->cwdh;
	while (mCwdh)
	{
		CWDH_s* bCwdh = (CWDH_s*)((u32)buffer + ((u32)mCwdh - base));
		unpatchPtr((void**)&bCwdh->next, base, size);
		mCwdh = mCwdh->next;
	}

	CMAP_s* mCmap = mFinf->cmap;
	while (mCmap)
	{
		CMAP_s* bCmap = (CMAP_s*)((u32)buffer + ((u32)mCmap - base));
		unpatchPtr((void**)&bCmap->next, base, size);
		mCmap = mCmap->next;
	}

	if (patchError) {
	    printf("Patching errors occurred. Font is likely invalid.\n");
	}

	mkdir(SYSFONT_OUT_DIR, 0777);

	FILE* f = fopen(SYSFONT_OUT_PATH, "wb");
	if (f)
	{
		fwrite(buffer, 1, size, f);
		fclose(f);
		printf("Successfully dumped to %s\n", SYSFONT_OUT_PATH);
		if (!patchError) {
		    printf("\nPress START to exit\n");
		    succeeded = 1;
		}
	}
	else
	{
		printf("Failed to open file for writing!\n");
	}
	free(buffer);
}

int main(int argc, char* argv[])
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	printf("3DS System Font Dumper\n");
	printf("Press A to dump font\n");
	printf("Press START to exit\n\n");

	while (aptMainLoop())
	{
		gspWaitForVBlank();
		gfxSwapBuffers();
		hidScanInput();

		u32 kDown = hidKeysDown();
		if (kDown & KEY_A && succeeded == 0)
		{
			dumpSystemFont();
		}
		if (kDown & KEY_START)
			break;
	}

	gfxExit();
	return 0;
}
