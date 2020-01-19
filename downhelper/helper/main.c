#include <pspsdk.h>
#include <pspkernel.h>
#include <pspnand_driver.h>

#include <string.h>
#include <stdio.h>
#include <malloc.h>


PSP_MODULE_INFO("Downdater_Helper", 0x1000, 1, 1);

PSP_MAIN_THREAD_ATTR(0);



#define printf pspDebugScreenPrintf

#define N_MODULES	7

#define PSP_SIGNATURE	0x5053507E

void create_dirs()
{
	sceIoMkdir("ms0:/DOWNDATER", 0777);
	sceIoMkdir("ms0:/DOWNDATER/PRX", 0777);
	sceIoMkdir("ms0:/DOWNDATER/DUMP", 0777);
	sceIoMkdir("ms0:/DOWNDATER/DUMP/data", 0777);
	sceIoMkdir("ms0:/DOWNDATER/DUMP/data/cert", 0777);
	sceIoMkdir("ms0:/DOWNDATER/DUMP/dic", 0777);
	sceIoMkdir("ms0:/DOWNDATER/DUMP/font", 0777);
	sceIoMkdir("ms0:/DOWNDATER/DUMP/kd", 0777);
	sceIoMkdir("ms0:/DOWNDATER/DUMP/kd/resource", 0777);
	sceIoMkdir("ms0:/DOWNDATER/DUMP/vsh", 0777);
	sceIoMkdir("ms0:/DOWNDATER/DUMP/vsh/etc", 0777);
	sceIoMkdir("ms0:/DOWNDATER/DUMP/vsh/module", 0777);
	sceIoMkdir("ms0:/DOWNDATER/DUMP/vsh/resource", 0777);
}

SceUID outputfilelist=-1, inputfilelist=-1;

void ErrorExit(char *error)
{
	printf("%s\n", error);

	sceKernelDelayThread(4 * 1000 * 1000);
	sceKernelExitGame();	
}

void writeOutputListEntry(char *str)
{
	char entry[1024];

	strcpy(entry, str);

	entry[5] = '8';
	
	sceIoWrite(outputfilelist, entry, strlen(entry)+1);
}

void writeInputListEntry(char *str)
{
	sceIoWrite(inputfilelist, str, strlen(str)+1);
}

char buffer[32*1024];

void copyfile(char *input, char *output)
{
	SceUID infd = sceIoOpen(input, PSP_O_RDONLY, 0777);
	SceUID outfd = sceIoOpen(output, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);

	int bytesread;

	printf("Copying %s to %s\n", input, output);

	if (infd < 0 || outfd < 0)
	{
		ErrorExit("Error copying file.\n");
	}

	while ((bytesread = sceIoRead(infd, buffer, 32*1024)) > 0)
	{
		sceIoWrite(outfd, buffer, bytesread);
	}

	sceIoClose(infd);
	sceIoClose(outfd);
}

char inputpath[1024], outputpath[1024];

void dump_dir(char *path)
{
	int dfd;

	dfd = sceIoDopen(path);

	if (dfd < 0)
	{
		printf("Error opeining dir %s\n", path);
		ErrorExit("");
		return;
	}

	SceIoDirent dir;

	while(sceIoDread(dfd, &dir) > 0)
	{
		if(dir.d_stat.st_attr & FIO_SO_IFDIR)
		{
			if(dir.d_name[0] != '.')
			{
				char nextdir[1024];

				sprintf(nextdir, "%s%s/", path, dir.d_name);

				dump_dir(nextdir);
			}
		}
		else
		{
			sprintf(inputpath, "%s%s", path, dir.d_name);
			sprintf(outputpath, "ms0:/DOWNDATER/DUMP/%s", inputpath+8);
			writeOutputListEntry(inputpath); // Input now, output after
			writeInputListEntry(outputpath); // Output now, input after
			copyfile(inputpath, outputpath);
		}
	}
}

void dump_flash()
{

	outputfilelist = sceIoOpen("ms0:/DOWNDATER/outputfl.bin", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	inputfilelist = sceIoOpen("ms0:/DOWNDATER/inputfl.bin", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);

	if (inputfilelist < 0 || outputfilelist < 0)
		ErrorExit("Error creating list files.");

	dump_dir("flash0:/");

	char null = 0;

	sceIoWrite(inputfilelist, &null, 1);
	sceIoClose(inputfilelist);

	sceIoWrite(outputfilelist, &null, 1);
	sceIoClose(outputfilelist);
}


void dump_ipl100()
{
	int i;
	int ppb = sceNandGetPagesPerBlock();
	int ps = sceNandGetPageSize();
	char *block;
	SceUID fd;

	block = (char *)malloc(ppb*ps);

	if (!block)
	{
		ErrorExit("Error allocating memory.\n");
	}

	fd = sceIoOpen("ms0:/DOWNDATER/ipl.bin", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);

	if (fd < 0)
	{
		ErrorExit("Error creating ipl file.\n");
	}	

	for (i = 0x40000 / ps; i < 0x74000 / ps; i += ppb)
	{
		sceNandLock(0);
	
		if (sceNandReadBlockWithRetry(i, block, NULL) < 0)
		{
			ErrorExit("Error reading IPL block.\n");			
			
		}

		sceIoWrite(fd, block, ppb*ps);
		sceNandUnlock();
	}

	free(block);
	sceIoClose(fd);
}

int main()
{
	pspDebugScreenInit();

	printf("Creating dirs.\n");
	create_dirs();

	printf("Dumping ipl.\n");
	dump_ipl100();


	printf("Dumping flash0...\n");
	dump_flash();

	ErrorExit("Finished. Exiting in four seconds\n");

	return 0;
}

