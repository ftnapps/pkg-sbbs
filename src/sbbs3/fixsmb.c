/* fixsmb.c */

/* Synchronet message base (SMB) index re-generator */

/* $Id: fixsmb.c,v 1.30 2005/10/02 23:32:25 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2005 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include <stdio.h>	
#include <stdlib.h>	/* atoi, qsort */
#include <string.h>	/* strnicmp */
#include <ctype.h>	/* toupper */

#include "smblib.h"
#include "genwrap.h"	/* PLATFORM_DESC */
#include "str_list.h"	/* strList API */
#include "crc16.h"

smb_t	smb;
BOOL	renumber=FALSE;
char*	usage="usage: fixsmb [-renumber] <smb_file> [[smb_file] [...]]\n";

int compare_index(const idxrec_t* idx1, const idxrec_t* idx2)
{
	return(idx1->number - idx2->number);
}

void sort_index(smb_t* smb)
{
	ulong		l;
	idxrec_t*	idx;

	printf("Sorting index... ");
	if((idx=malloc(sizeof(idxrec_t)*smb->status.total_msgs))==NULL) {
		perror("malloc");
		return;
	}

	rewind(smb->sid_fp);
	for(l=0;l<smb->status.total_msgs;l++)
		if(fread(&idx[l],sizeof(idxrec_t),1,smb->sid_fp)<1) {
			perror("reading index");
			break;
		}

	qsort(idx,l,sizeof(idxrec_t)
		,(int(*)(const void*, const void*))compare_index);

	rewind(smb->sid_fp);
	chsize(fileno(smb->sid_fp),0L);			/* Truncate the index */

	printf("\nRe-writing index... \n");
	smb->status.total_msgs=l;
	for(l=0;l<smb->status.total_msgs;l++)
		if(fwrite(&idx[l],sizeof(idxrec_t),1,smb->sid_fp)<1) {
			perror("writing index");
			break;
		}

	free(idx);
	printf("\n");
}

void unlock_msgbase(void)
{
	int i;
	if(smb_islocked(&smb) && (i=smb_unlock(&smb))!=0)
		printf("smb_unlock returned %d: %s\n",i,smb.last_error);
}

int fixsmb(char* sub)
{
	char*		p;
	char*		text;
	char		c;
	int 		i,w;
	ulong		l,length,size,n;
	smbmsg_t	msg;

	memset(&smb,0,sizeof(smb));

	SAFECOPY(smb.file,sub);

	if((p=getfext(smb.file))!=NULL && stricmp(p,".shd")==0)
		*p=0;	/* Chop off .shd extension, if supplied on command-line */

	printf("Opening %s\n",smb.file);

	if((i=smb_open(&smb))!=0) {
		printf("smb_open returned %d: %s\n",i,smb.last_error);
		exit(1); 
	}

	if((i=smb_lock(&smb))!=0) {
		printf("smb_lock returned %d: %s\n",i,smb.last_error);
		exit(1);
	}

	if((i=smb_locksmbhdr(&smb))!=0) {
		smb_close(&smb);
		printf("smb_locksmbhdr returned %d: %s\n",i,smb.last_error);
		exit(1); 
	}

	if((i=smb_getstatus(&smb))!=0) {
		smb_unlocksmbhdr(&smb);
		smb_close(&smb);
		printf("smb_getstatus returned %d: %s\n",i,smb.last_error);
		exit(1); 
	}

	if(!(smb.status.attr&SMB_HYPERALLOC)) {

		if((i=smb_open_ha(&smb))!=0) {
			smb_close(&smb);
			printf("smb_open_ha returned %d: %s\n",i,smb.last_error);
			exit(1); 
		}

		if((i=smb_open_da(&smb))!=0) {
			smb_close(&smb);
			printf("smb_open_da returned %d: %s\n",i,smb.last_error);
			exit(1); 
		}

		rewind(smb.sha_fp);
		chsize(fileno(smb.sha_fp),0L);		/* Truncate the header allocation file */
		rewind(smb.sda_fp);
		chsize(fileno(smb.sda_fp),0L);		/* Truncate the data allocation file */
	}

	rewind(smb.sid_fp);
	chsize(fileno(smb.sid_fp),0L);			/* Truncate the index */


	if(!(smb.status.attr&SMB_HYPERALLOC)) {
		length=filelength(fileno(smb.sdt_fp));
		w=0;
		for(l=0;l<length;l+=SDT_BLOCK_LEN)	/* Init .SDA file to NULL */
			fwrite(&w,2,1,smb.sda_fp);

		length=filelength(fileno(smb.shd_fp));
		c=0;
		for(l=0;l<length;l+=SHD_BLOCK_LEN)	/* Init .SHD file to NULL */
			fwrite(&c,1,1,smb.sha_fp); 
	} else
		length=filelength(fileno(smb.shd_fp));

	n=0;	/* messsage offset */
	for(l=smb.status.header_offset;l<length;l+=size) {
		size=SHD_BLOCK_LEN;
		printf("\r%2lu%%  ",(long)(100.0/((float)length/l)));
		msg.idx.offset=l;
		if((i=smb_lockmsghdr(&smb,&msg))!=0) {
			printf("\n(%06lX) smb_lockmsghdr returned %d:\n%s\n",l,i,smb.last_error);
			continue; 
		}
		i=smb_getmsghdr(&smb,&msg);
		smb_unlockmsghdr(&smb,&msg);
		if(i!=0) {
			printf("\n(%06lX) smb_getmsghdr returned %d:\n%s\n",l,i,smb.last_error);
			continue; 
		}
		size=smb_hdrblocks(smb_getmsghdrlen(&msg))*SHD_BLOCK_LEN;
		printf("#%-5lu (%06lX) %-25.25s ",msg.hdr.number,l,msg.from);

		/* Create hash record */
		if(msg.hdr.attr&MSG_DELETE)
			text=NULL;
		else
			text=smb_getmsgtxt(&smb,&msg,GETMSGTXT_BODY_ONLY);
		i=smb_hashmsg(&smb,&msg,text,TRUE /* update */);
		if(i!=SMB_SUCCESS)
			printf("!ERROR %d hashing message\n", i);
		if(text!=NULL)
			free(text);

		/* Index the header */
		if(msg.hdr.attr&MSG_DELETE)
			printf("Not indexing deleted message\n");
		else if(msg.hdr.number==0)
			printf("Not indexing invalid message number (0)!\n");
		else {   
			msg.offset=n;
			if(renumber)
				msg.hdr.number=n+1;
			if(msg.hdr.netattr&MSG_INTRANSIT) {
				printf("Removing 'in transit' attribute\n");
				msg.hdr.netattr&=~MSG_INTRANSIT;
			}
			if((i=smb_putmsg(&smb,&msg))!=0) {
				printf("\nsmb_putmsg returned %d: %s\n",i,smb.last_error);
				continue; 
			}
			n++; 
		}

		if(!(smb.status.attr&SMB_HYPERALLOC)) {
			/**************************/
			/* Allocate header blocks */
			/**************************/
			fseek(smb.sha_fp,(l-smb.status.header_offset)/SHD_BLOCK_LEN,SEEK_SET);
			if(msg.hdr.attr&MSG_DELETE) c=0;		/* mark as free */
			else c=1;								/* or allocated */

			for(i=0;i<(int)(size/SHD_BLOCK_LEN);i++)
				fputc(c,smb.sha_fp);

			/************************/
			/* Allocate data blocks */
			/************************/

			if(!(msg.hdr.attr&MSG_DELETE))
				smb_incmsg_dfields(&smb,&msg,1);
		}

		smb_freemsgmem(&msg); 
	}
	printf("\r%79s\r100%%\n","");
	smb.status.total_msgs=n;
	if(renumber)
		smb.status.last_msg=n;
	else
		sort_index(&smb);
	printf("Saving message base status (%lu total messages).\n",n);
	if((i=smb_putstatus(&smb))!=0)
		printf("\nsmb_putstatus returned %d: %s\n",i,smb.last_error);
	smb_unlocksmbhdr(&smb);
	printf("Closing message base.\n");
	smb_close(&smb);
	unlock_msgbase();
	printf("Done.\n");
	return(0);
}

int main(int argc, char **argv)
{
	char		revision[16];
	int 		i;
	str_list_t	list;

	sscanf("$Revision: 1.30 $", "%*s %s", revision);

	printf("\nFIXSMB v2.10-%s (rev %s) SMBLIB %s - Rebuild Synchronet Message Base\n\n"
		,PLATFORM_DESC,revision,smb_lib_ver());

	list=strListInit();

	for(i=1;i<argc;i++) {
		if(argv[i][0]=='-') {
			if(!stricmp(argv[i],"-renumber"))
				renumber=TRUE;
		} else
			strListPush(&list,argv[i]);
	}

	if(!strListCount(list)) {
		printf(usage);
		exit(1); 
	}

	atexit(unlock_msgbase);

	for(i=0;list[i]!=NULL;i++)
		fixsmb(list[i]);

	return(0);
}
