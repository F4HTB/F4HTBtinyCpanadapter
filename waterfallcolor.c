#include <dirent.h>

int indexlistofcolorfile = 0,indexlistofcolorfilemax=0;
char listofcolorfile[20][20];

short int colormap_rainbow[256][3] = {
{0,0,127},
{0,0,131},
{0,0,135},
{0,0,139},
{0,0,143},
{0,0,147},
{0,0,151},
{0,0,155},
{0,0,159},
{0,0,163},
{0,0,167},
{0,0,171},
{0,0,175},
{0,0,179},
{0,0,183},
{0,0,187},
{0,0,191},
{0,0,195},
{0,0,199},
{0,0,203},
{0,0,207},
{0,0,211},
{0,0,215},
{0,0,219},
{0,0,223},
{0,0,227},
{0,0,231},
{0,0,235},
{0,0,239},
{0,0,243},
{0,0,247},
{0,0,251},
{0,0,255},
{0,4,255},
{0,8,255},
{0,12,255},
{0,16,255},
{0,20,255},
{0,24,255},
{0,28,255},
{0,32,255},
{0,36,255},
{0,40,255},
{0,44,255},
{0,48,255},
{0,52,255},
{0,56,255},
{0,60,255},
{0,64,255},
{0,68,255},
{0,72,255},
{0,76,255},
{0,80,255},
{0,84,255},
{0,88,255},
{0,92,255},
{0,96,255},
{0,100,255},
{0,104,255},
{0,108,255},
{0,112,255},
{0,116,255},
{0,120,255},
{0,124,255},
{0,128,255},
{0,132,255},
{0,136,255},
{0,140,255},
{0,144,255},
{0,148,255},
{0,152,255},
{0,156,255},
{0,160,255},
{0,164,255},
{0,168,255},
{0,172,255},
{0,176,255},
{0,180,255},
{0,184,255},
{0,188,255},
{0,192,255},
{0,196,255},
{0,200,255},
{0,204,255},
{0,208,255},
{0,212,255},
{0,216,255},
{0,220,255},
{0,224,255},
{0,228,255},
{0,232,255},
{0,236,255},
{0,240,255},
{0,244,255},
{0,248,255},
{0,252,255},
{1,255,253},
{5,255,249},
{9,255,245},
{13,255,241},
{17,255,237},
{21,255,233},
{25,255,229},
{29,255,225},
{33,255,221},
{37,255,217},
{41,255,213},
{45,255,209},
{49,255,205},
{53,255,201},
{57,255,197},
{61,255,193},
{65,255,189},
{69,255,185},
{73,255,181},
{77,255,177},
{81,255,173},
{85,255,169},
{89,255,165},
{93,255,161},
{97,255,157},
{101,255,153},
{105,255,149},
{109,255,145},
{113,255,141},
{117,255,137},
{121,255,133},
{125,255,129},
{129,255,125},
{133,255,121},
{137,255,117},
{141,255,113},
{145,255,109},
{149,255,105},
{153,255,101},
{157,255,97},
{161,255,93},
{165,255,89},
{169,255,85},
{173,255,81},
{177,255,77},
{181,255,73},
{185,255,69},
{189,255,65},
{193,255,61},
{197,255,57},
{201,255,53},
{205,255,49},
{209,255,45},
{213,255,41},
{217,255,37},
{221,255,33},
{225,255,29},
{229,255,25},
{233,255,21},
{237,255,17},
{241,255,13},
{245,255,9},
{249,255,5},
{253,255,1},
{255,252,0},
{255,248,0},
{255,244,0},
{255,240,0},
{255,236,0},
{255,232,0},
{255,228,0},
{255,224,0},
{255,220,0},
{255,216,0},
{255,212,0},
{255,208,0},
{255,204,0},
{255,200,0},
{255,196,0},
{255,192,0},
{255,188,0},
{255,184,0},
{255,180,0},
{255,176,0},
{255,172,0},
{255,168,0},
{255,164,0},
{255,160,0},
{255,156,0},
{255,152,0},
{255,148,0},
{255,144,0},
{255,140,0},
{255,136,0},
{255,132,0},
{255,128,0},
{255,124,0},
{255,120,0},
{255,116,0},
{255,112,0},
{255,108,0},
{255,104,0},
{255,100,0},
{255,96,0},
{255,92,0},
{255,88,0},
{255,84,0},
{255,80,0},
{255,76,0},
{255,72,0},
{255,68,0},
{255,64,0},
{255,60,0},
{255,56,0},
{255,52,0},
{255,48,0},
{255,44,0},
{255,40,0},
{255,36,0},
{255,32,0},
{255,28,0},
{255,24,0},
{255,20,0},
{255,16,0},
{255,12,0},
{255,8,0},
{255,4,0},
{255,0,0},
{251,0,0},
{247,0,0},
{243,0,0},
{239,0,0},
{235,0,0},
{231,0,0},
{227,0,0},
{223,0,0},
{219,0,0},
{215,0,0},
{211,0,0},
{207,0,0},
{203,0,0},
{199,0,0},
{195,0,0},
{191,0,0},
{187,0,0},
{183,0,0},
{179,0,0},
{175,0,0},
{171,0,0},
{167,0,0},
{163,0,0},
{159,0,0},
{155,0,0},
{151,0,0},
{147,0,0},
{143,0,0},
{139,0,0},
{135,0,0},
{131,0,0},
{127,0,0}
};



void scandirfilecolor()
/* Scans a directory and retrieves all files of given extension */
{
    DIR *d = NULL;
    struct dirent *dir = NULL;
	int ret,i=0;
	char *p;
    d = opendir("./");
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
			
            p=strtok(dir->d_name,".");
            p=strtok(NULL,".");
            if(p!=NULL)
            {
                ret=strcmp(p,"256");
                if(ret==0)
                {
				printf("%s,",dir->d_name);
				strcpy( listofcolorfile[i], dir->d_name);
				indexlistofcolorfilemax=i;
				i++;
			 }
        }
		}
        closedir(d);
		printf("\n");
   } 
}



void read_csv(char *filename){
	FILE *file;
	file = fopen(filename, "r");
	int i = 0;
    char line[15];
	while (fgets(line, 15, file) && (i < 256))
    {
        char* tmp = strdup(line);

		const char* tok;
		tok = strtok(tmp, ";");
		colormap_rainbow[i][0]=(short int)atof(tok);
		tok = strtok(NULL, ";");
		colormap_rainbow[i][1]=(short int)atof(tok);
		tok = strtok(NULL, ";");
		colormap_rainbow[i][2]=(short int)atof(tok);
   
		//printf("%d %d %d\n", colormap_rainbow[i][0],colormap_rainbow[i][1],colormap_rainbow[i][2]);

        free(tmp);
        i++;
    }
}