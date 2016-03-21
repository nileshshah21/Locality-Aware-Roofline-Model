#include "roofline.h"

void roofline_progress_set(struct roofline_progress_bar * bar, char * info, size_t low, size_t curr, size_t up){
    if(bar != NULL){
	bar->info = info;
	bar->begin = low;
	bar->current = curr;
	bar->end = up;
    }
    roofline_progress_print(bar);		
}

/*
 * Print the progress of a roofline for one memory level and one type of micro code.
 * info [====>            ][ 76%] 
 */
void roofline_progress_print(struct roofline_progress_bar * progress){
    unsigned cols;
    FILE * cmd_f;
    size_t percent;
    unsigned bar_len, fill_len, i;
    char cols_str[16];
    
    memset(cols_str,0,sizeof(cols_str));

    /* get term width */
    cols = 100;
    cmd_f = popen("tput cols","r");
    if(fread(cols_str,sizeof(*cols_str),sizeof(cols_str),cmd_f) == 0)
	perror("fread");
    else if(strcmp(cols_str,""))
	cols = atoi(cols_str);
     
    pclose(cmd_f);
    percent = 100 * (progress->current - progress->begin)/ (double)(progress->end - progress->begin);
    /* compute bars length */
    bar_len = cols;
    bar_len -= 1+(progress->info != NULL ? strlen(progress->info) : 0); /* progress bar is after info */
    bar_len -= 7;                        /* progress bar is before contexts percent */
    bar_len -= 2;                        /* progress bar is between brackets */
    fill_len = bar_len;
    fill_len -= 1;                       /* filled region is preceeded by '>' */
    fill_len = fill_len * percent / 100;

    /* print begin */
    printf("\r%s [",progress->info != NULL ? progress->info : "");
    /* print progress */
    for(i=0;i<fill_len;i++)
	printf("=");
    printf(">");
    /* fill progress bar with space */
    for(i=fill_len+1;i<bar_len;i++)
	printf(" ");
    /* print progress percentage */
    printf("] ");
    printf("[%3ld%%]", percent);
    fflush(stdout);
}

void roofline_progress_clean(void){
    unsigned cols;
    FILE * cmd_f;
    char cols_str[16];
    
    memset(cols_str,0,sizeof(cols_str));

    /* get term width */
    cols = 100;
    cmd_f = popen("tput cols","r");
    if(fread(cols_str,sizeof(*cols_str),sizeof(cols_str),cmd_f) == 0)
	perror("fread");
    else if(strcmp(cols_str,""))
	cols = atoi(cols_str);
     
    pclose(cmd_f);
    printf("\r%*s\r", cols, " ");
}

