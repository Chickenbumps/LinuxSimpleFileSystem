//
// Simple FIle System

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* optional */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
/***********/

#include "sfs_types.h"
#include "sfs_func.h"
#include "sfs_disk.h"
#include "sfs.h"

void dump_directory();

/* BIT operation Macros */
/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (1<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1<<(b)))
#define BIT_CHECK(a,b) ((a) & (1<<(b)))

static struct sfs_super spb;	// superblock
static struct sfs_dir sd_cwd = { SFS_NOINO }; // current working directory

void error_message(const char *message, const char *path, int error_code) {
	switch (error_code) {
	case -1:
		printf("%s: %s: No such file or directory\n",message, path); return;
	case -2:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -3:
		printf("%s: %s: Directory full\n",message, path); return;
	case -4:
		printf("%s: %s: No block available\n",message, path); return;
	case -5:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -6:
		printf("%s: %s: Already exists\n",message, path); return;
	case -7:
		printf("%s: %s: Directory not empty\n",message, path); return;
	case -8:
		printf("%s: %s: Invalid argument\n",message, path); return;
	case -9:
		printf("%s: %s: Is a directory\n",message, path); return;
	case -10:
		printf("%s: %s: Is not a file\n",message, path); return;
	default:
		printf("unknown error code\n");
		return;
	}
}

void sfs_mount(const char* path)
{
	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}

	printf("Disk image: %s\n", path);

	disk_open(path);
	disk_read( &spb, SFS_SB_LOCATION );

	printf("Superblock magic: %x\n", spb.sp_magic);

	assert( spb.sp_magic == SFS_MAGIC );
	
	printf("Number of blocks: %d\n", spb.sp_nblocks);
	printf("Volume name: %s\n", spb.sp_volname);
	printf("%s, mounted\n", spb.sp_volname);
	
	sd_cwd.sfd_ino = 1;		//init at root
	sd_cwd.sfd_name[0] = '/';
	sd_cwd.sfd_name[1] = '\0';
}

void sfs_umount() {

	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}
}


void sfs_touch(const char* path)
{
	//skeleton implementation

	struct sfs_inode si;
	disk_read( &si, sd_cwd.sfd_ino );

	//for consistency
	assert( si.sfi_type == SFS_TYPE_DIR );

	//we assume that cwd is the root directory and root directory is empty which has . and .. only
	//unused DISK2.img satisfy these assumption
	//for new directory entry(for new file), we use cwd.sfi_direct[0] and offset 2
	//becasue cwd.sfi_directory[0] is already allocated, by .(offset 0) and ..(offset 1)
	//for new inode, we use block 6 
	// block 0: superblock,	block 1:root, 	block 2:bitmap 
	// block 3:bitmap,  	block 4:bitmap 	block 5:root.sfi_direct[0] 	block 6:unused
	//
	//if used DISK2.img is used, result is not defined

    //buffer for disk read
    struct sfs_dir sd[SFS_DENTRYPERBLOCK];
    int c;

    // Error1: is there same path ?
    int samePath = 0;
    for (c = 0; c < SFS_NDIRECT; c++) {
        disk_read(sd,si.sfi_direct[c]);
        int d;
        for(d=0; d < SFS_DENTRYPERBLOCK;d++) {
            if(strcmp(sd[d].sfd_name,path) == 0){
                samePath = 1;
            }
        }
    }
    if(samePath == 1){
        error_message("touch",path,-6);
        return;
    }
    int bm[4096]={0,};
    int real[4096]={0,};
    int temp[4096]={0,};
    int afbm[4096];

    int bmNum = 0;
    if(spb.sp_nblocks%4096 == 0){
        bmNum = spb.sp_nblocks/4096;
    }else{
        bmNum = spb.sp_nblocks/4096 + 1;
    }

    int a;
    int curbm = 0;
    int bitFull = 1;
    for(a =2;a < bmNum; a++){
        disk_read(bm,a);
        if(bm[127]!= -1){
            bitFull = 0;
            curbm = a;
            break;
        }
    }
    // Error 2: bitmap Full
    if(bitFull == 1){
        error_message("touch",path,-4);
        return;
    }
    // Error3: directory is full
    disk_read(sd,si.sfi_direct[14]);
    if(sd[7].sfd_ino != 0){
        error_message("touch",path,-3);
        return ;
    }
    int x;
    for (x = 0;x <4096 ;x++) {
        afbm[x] = bm[x];
    }
//    int m,n;
//    for (m = 0; m < 512; ++m) {
//        for (n = 0; n < 8; ++n) {
//            printf("%d",bm[8*m+n]);
//        }
//        printf("\n");
//    }
    int p = 0;
    int k = 0;
    int i,j;
    int position;
    int find = 0;
    while(afbm[k] == -1){
        k++;
    }
    while (1){
        temp[p] = bm[k] % 2;
        bm[k] = bm[k] /2;
        p++;
        if(bm[k] ==0)   break;
    }
    for(i = p -1; i >=0; i--) {
        real[p-i-1] = temp[i];
    }

    int flag = 0;
    for (i = 0; i< 512; i++) {
        for (j = 0; j < 8; ++j) {
            if(real[8*i+j] == 0){
                position = k*32+8*i+j;
                real[8*i+j] = 1;
                flag = 1;
                break;
            }
        }
        if(flag == 1){
            break;
        }
    }
    BIT_FLIP(afbm[k],position);
    disk_write(afbm,curbm);

    //allocate new block
    int newbie_ino = position;
    int l;
    int location = 100;
    int v;
    int directNum;
    int nextDirect = 0;
    for(v =0;v<SFS_NDIRECT;v++){
        //block access
        if(si.sfi_direct[v] == 0){
            location = 0;
            directNum = v;
            nextDirect = 1;
            break;
        }
        disk_read(sd, si.sfi_direct[v]);

        if(v == 0){
            for (l = 2; l <8 ; l++) {
                if (sd[l].sfd_ino == 0) {
                    location = l;
                    break;
                }
            }
        }
        else {
            for (l = 1; l < 8; l++) {
                if (sd[l].sfd_ino == 0) {
                    location = l;
                    break;
                }
            }
        }
        if(location != 100){
            directNum = v;
            break;
        }
    }

    if(nextDirect == 1){
        int m;
        for (m = 0; m < 4096; m++) {
            bm[m] = 0;
            real[m]= 0;
            temp[m] = 0;
        }
        bitFull = 1;
        for(a =2;a < bmNum; a++){
            disk_read(bm,a);
            if(bm[127]!= -1){
                bitFull = 0;
                curbm = a;
                break;
            }
        }
        // Error 2: bitmap Full
        if(bitFull == 1){
            error_message("touch",path,-4);
            return;
        }
        for (x = 0;x <4096 ;x++) {
            afbm[x] = bm[x];
        }
        while(afbm[k] == -1){
            k++;
        }
        while (1){
            temp[p] = bm[k] % 2;
            bm[k] = bm[k] /2;
            p++;
            if(bm[k] ==0)   break;
        }
        for(i = p -1; i >=0; i--) {
            real[p-i-1] = temp[i];
        }

        flag = 0;
        int nextPosition;
        for (i = 0; i< 512; i++) {
            for (j = 0; j < 8; ++j) {
                if(real[8*i+j] == 0){
                    nextPosition = 32*k+8*i+j;
                    real[8*i+j] = 1;
                    flag = 1;
                    break;
                }
            }
            if(flag == 1){
                break;
            }
        }
        BIT_FLIP(afbm[k],nextPosition);
        disk_write(afbm,curbm);

        newbie_ino = nextPosition;

        si.sfi_direct[directNum] = position;
        disk_write( &si, sd_cwd.sfd_ino );
        struct sfs_inode newD;
        bzero(&newD,SFS_BLOCKSIZE);
        newD.sfi_size = 0;
        disk_write(&newD,position);
        disk_read( &si, sd_cwd.sfd_ino );
        disk_read( sd, si.sfi_direct[directNum] );
    }
    sd[location].sfd_ino = newbie_ino;
    strncpy( sd[location].sfd_name, path, SFS_NAMELEN );

	disk_write( sd, si.sfi_direct[directNum] );

	si.sfi_size += sizeof(struct sfs_dir);
	disk_write( &si, sd_cwd.sfd_ino );

	struct sfs_inode newbie;

	bzero(&newbie,SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	newbie.sfi_size = 0;
	newbie.sfi_type = SFS_TYPE_FILE;
	disk_write( &newbie, newbie_ino );
	//
	//disk_read(sd,si.sfi_direct[directNum]);

}

void sfs_cd(const char* path)
{
    struct sfs_inode c_inode;

    disk_read(&c_inode, sd_cwd.sfd_ino);
    int i;
    int errorFlag = 1;// is dir?
    int isfile = 1;
    struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
    if(path == NULL){
        errorFlag = 0;
        isfile = 0;
        strcpy(sd_cwd.sfd_name,"/");
        sd_cwd.sfd_ino = 1;
    }
    else if (c_inode.sfi_type == SFS_TYPE_DIR) {
        for(i=0; i < SFS_NDIRECT; i++) {
            if(c_inode.sfi_type != SFS_TYPE_DIR) break;
            if (c_inode.sfi_direct[i] == 0) break;
            disk_read(dir_entry, c_inode.sfi_direct[i]);
            int j;
            struct sfs_inode tnode;
            for(j=0; j < SFS_DENTRYPERBLOCK;j++) {
                disk_read(&tnode,dir_entry[j].sfd_ino);
                if(strcmp(dir_entry[j].sfd_name,path) == 0){
                    errorFlag = 0;
                    if(tnode.sfi_type == SFS_TYPE_DIR){
                        isfile = 0;
                        strcpy(sd_cwd.sfd_name, dir_entry[j].sfd_name);
                        sd_cwd.sfd_ino = dir_entry[j].sfd_ino;
                        break;
                    }
                }
            }
        }
    }
    if(errorFlag == 1){
        error_message("cd",path,-1); //
    }
    if(errorFlag == 0 && isfile == 1){
        error_message("cd",path,-2); // 폴더 아님
    }
}

void sfs_ls(const char* path)
{
    struct sfs_inode c_inode;

    disk_read(&c_inode, sd_cwd.sfd_ino);
    int i;
    struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
    int errorflag = 1;
    if (c_inode.sfi_type == SFS_TYPE_DIR) {
        for(i=0; i < SFS_NDIRECT; i++) {
            if (c_inode.sfi_direct[i] == 0) break;
            disk_read(dir_entry, c_inode.sfi_direct[i]);
            int j;
            for(j=0; j < SFS_DENTRYPERBLOCK;j++) {
                if(path != NULL){
                    if(strcmp(dir_entry[j].sfd_name,path) == 0){
                        errorflag = 0;
                        struct sfs_inode inode;
                        disk_read(&inode,dir_entry[j].sfd_ino);
                        if(inode.sfi_type == SFS_TYPE_FILE){
                            printf("%s",dir_entry[j].sfd_name);
                            break;
                        }
                        struct sfs_dir tdir_entry[SFS_DENTRYPERBLOCK];
                        int k;
                        for (k = 0; k < SFS_NDIRECT; k++) {
                            if (inode.sfi_direct[k] == 0) break;
                            disk_read(tdir_entry, inode.sfi_direct[k]);
                            int m;
                            for (m = 0; m < SFS_DENTRYPERBLOCK; m++) {
                                if (tdir_entry[m].sfd_ino != 0) {
                                    struct sfs_inode tnode;
                                    disk_read(&tnode,tdir_entry[m].sfd_ino);
                                    if (tnode.sfi_type == SFS_TYPE_FILE) {
                                        printf("%s\t", tdir_entry[m].sfd_name);
                                    } else if (tnode.sfi_type == SFS_TYPE_DIR) {
                                        printf("%s/\t", tdir_entry[m].sfd_name);
                                    }
                                }
                            }
                        }

                    }
                }else{
                    errorflag = 0;
                    if(dir_entry[j].sfd_ino != 0){
                        struct sfs_inode tnode;
                        disk_read(&tnode,dir_entry[j].sfd_ino);
                        if (tnode.sfi_type == SFS_TYPE_FILE){
                            printf("%s\t",dir_entry[j].sfd_name);
                        }
                        else if(tnode.sfi_type == SFS_TYPE_DIR){
                            printf("%s/\t",dir_entry[j].sfd_name);
                        }
                    }
                }
            }
        }
    }
    if(errorflag){
        error_message("ls",path,-1);
    }else{
        printf("\n");
    }
}

void sfs_mkdir(const char* org_path) 
{
    //skeleton implementation

    struct sfs_inode si;
    disk_read( &si, sd_cwd.sfd_ino );

    //for consistency
    assert( si.sfi_type == SFS_TYPE_DIR );

    //we assume that cwd is the root directory and root directory is empty which has . and .. only
    //unused DISK2.img satisfy these assumption
    //for new directory entry(for new file), we use cwd.sfi_direct[0] and offset 2
    //becasue cwd.sfi_directory[0] is already allocated, by .(offset 0) and ..(offset 1)
    //for new inode, we use block 6
    // block 0: superblock,	block 1:root, 	block 2:bitmap
    // block 3:bitmap,  	block 4:bitmap 	block 5:root.sfi_direct[0] 	block 6:unused
    //
    //if used DISK2.img is used, result is not defined

    //buffer for disk read
    struct sfs_dir sd[SFS_DENTRYPERBLOCK];
    int c;

    // Error1: is there same path ?
    int samePath = 0;
    for (c = 0; c < SFS_NDIRECT; c++) {
        disk_read(sd,si.sfi_direct[c]);
        int d;
        for(d=0; d < SFS_DENTRYPERBLOCK;d++) {
            if(strcmp(sd[d].sfd_name,org_path) == 0){
                samePath = 1;
            }
        }
    }
    if(samePath == 1){
        error_message("mkdir",org_path,-6);
        return;
    }
    int bm[4096]={0,};
    int real[4096]={0,};
    int temp[4096]={0,};
    int afbm[4096];

    int bmNum = 0;
    if(spb.sp_nblocks%4096 == 0){
        bmNum = spb.sp_nblocks/4096;
    }else{
        bmNum = spb.sp_nblocks/4096 + 1;
    }

    int a;
    int curbm = 0;
    int bitFull = 1;
    for(a =2;a < bmNum; a++){
        disk_read(bm,a);
        if(bm[127]!= -1){
            bitFull = 0;
            curbm = a;
            break;
        }
    }
    // Error 2: bitmap Full
    if(bitFull == 1){
        error_message("mkdir",org_path,-4);
        return;
    }

    // Error3: directory is full
    disk_read(sd,si.sfi_direct[14]);
    if(sd[7].sfd_ino != 0){
        error_message("mkdir",org_path,-3);
        return ;
    }

    int x;
    for (x = 0;x <4096 ;x++) {
        afbm[x] = bm[x];
    }

    int p = 0;
    int k = 0;
    int i,j;
    int position;
    int find = 0;
    while(afbm[k] == -1){
        k++;
    }

    while (1){
        temp[p] = bm[k] % 2;
        bm[k] = bm[k] /2;
        p++;
        if(bm[k] ==0){
            break;
        }
    }
    for(i = p -1; i >=0; i--) {
        real[p-i-1] = temp[i];
    }

    int flag = 0;
    for (i = 0; i< 512; i++) {
        for (j = 0; j < 8; ++j) {
            if(real[8*i+j] == 0){
                position = k*32+8*i+j;
                real[8*i+j] = 1;
                flag = 1;
                break;
            }
        }
        if(flag == 1){
            break;
        }
    }
//    printf("1curbm%d\n",curbm);
//    printf("1before<< :afbm[%d]:%d\n",k,afbm[k]);
    BIT_FLIP(afbm[k],position);
//    printf("1after<< :afbm[%d]:%d\n",k,afbm[k]);
    disk_write(afbm,curbm);


    //allocate new block
    int newbie_ino = position;
    int l;
    int location = 100;
    int v;
    int directNum;
    int nextDirect = 0;
    for(v =0;v<SFS_NDIRECT;v++){
        //block access
        if(si.sfi_direct[v] == 0){
            location = 0;
            directNum = v;
            nextDirect = 1;
            break;
        }
        disk_read(sd, si.sfi_direct[v]);

        if(v == 0){
            for (l = 2; l <8 ; l++) {
                if (sd[l].sfd_ino == 0) {
                    location = l;
                    break;
                }
            }
        }
        else {
            for (l = 1; l < 8; l++) {
                if (sd[l].sfd_ino == 0) {
                    location = l;
                    break;
                }
            }
        }
        if(location != 100){
            directNum = v;
            break;
        }
    }

    // direct over
    if(nextDirect == 1){
        int dbm[4096]={0,};
        int dreal[4096]={0,};
        int dtemp[4096]={0,};
        int dafbm[4096];
        bitFull = 1;
        for(a =2;a < bmNum; a++){
            disk_read(dbm,a);
            if(dbm[127]!= -1){
                bitFull = 0;
                curbm = a;
                break;
            }
        }
        // Error 2: bitmap Full
        if(bitFull == 1){
            error_message("mkdir",org_path,-4);
            return;
        }

        for (x = 0;x <4096 ;x++) {
            dafbm[x] = dbm[x];
        }
        while(dafbm[k] == -1){
            k++;
        }
        while (1){
            dtemp[p] = dbm[k] % 2;
            dbm[k] = dbm[k] /2;
            p++;
            if(dbm[k] ==0)   break;
        }
        for(i = p -1; i >=0; i--) {
            dreal[p-i-1] = dtemp[i];
        }
        flag = 0;
        int nextPosition;
        for (i = 0; i< 512; i++) {
            for (j = 0; j < 8; ++j) {
                if(dreal[8*i+j] == 0){
                    nextPosition = k*32+8*i+j;
                    dreal[8*i+j] = 1;
                    flag = 1;
                    break;
                }
            }
            if(flag == 1){
                break;
            }
        }
//        printf("d curbm%d\n",curbm);
//        printf("d before 1<< :afbm[%d]:%d\n",k,dafbm[k]);
        BIT_FLIP(dafbm[k],nextPosition);
//        printf("d after 1<< :afbm[%d]:%d\n",k,dafbm[k]);
        disk_write(dafbm,curbm);

        newbie_ino = nextPosition;

        si.sfi_direct[directNum] = position;
        disk_write( &si, sd_cwd.sfd_ino );
        struct sfs_inode newD;
        bzero(&newD,SFS_BLOCKSIZE);
        newD.sfi_size = 0;
        disk_write(&newD,position);
        disk_read( &si, sd_cwd.sfd_ino );
        disk_read( sd, si.sfi_direct[directNum] );
    }


    int f;
    for (f = 0; f<4096; f++) {
        bm[f] = 0;
        real[f] = 0;
        temp[f] = 0;
    }

    bitFull = 1;
    for(a =2;a < bmNum; a++){
        disk_read(bm,a);
        if(bm[127]!= -1){
            bitFull = 0;
            curbm = a;
            break;
        }
    }
    // Error 2: bitmap Full
    if(bitFull == 1){
        error_message("mkdir",org_path,-4);
        return;
    }
    for (x = 0;x <4096 ;x++) {
        afbm[x] = bm[x];
    }

    p = 0;
    k = 0;
    while(afbm[k] == -1){
        k++;
    }
    while (1){
        temp[p] = bm[k] % 2;
        bm[k] = bm[k] /2;
        p++;
        if(bm[k] ==0)   break;
    }
    for(i = p -1; i >=0; i--) {
        real[p-i-1] = temp[i];
    }

    flag = 0;
    int nnPosition;
    for (i = 0; i< 512; i++) {
        for (j = 0; j < 8; ++j) {
            if(real[8*i+j] == 0){
                nnPosition = k*32+8*i+j;
                real[8*i+j] = 1;
                flag = 1;
                break;
            }
        }
        if(flag == 1){
            break;
        }
    }
//    printf("2curbm%d\n",curbm);
//    printf("2before 1<< :afbm[%d]:%d\n",k,afbm[k]);
    BIT_FLIP(afbm[k],nnPosition);
//    printf("2after 1<< :afbm[%d]:%d\n",k,afbm[k]);
    disk_write(afbm,curbm);


    sd[location].sfd_ino = newbie_ino;
    strncpy( sd[location].sfd_name, org_path, SFS_NAMELEN );

    disk_write( sd, si.sfi_direct[directNum] );

    si.sfi_size += sizeof(struct sfs_dir);
    disk_write( &si, sd_cwd.sfd_ino );

    struct sfs_inode newbie;

    bzero(&newbie,SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
    newbie.sfi_size = newbie.sfi_size + 2* sizeof(struct sfs_dir);
    newbie.sfi_type = SFS_TYPE_DIR;
    newbie.sfi_direct[0] = newbie_ino + 1;
    disk_write( &newbie, newbie_ino );

    disk_read(sd,newbie.sfi_direct[0]);
    bzero(&sd,SFS_BLOCKSIZE);
    sd[0].sfd_ino = newbie_ino;
    strcpy(sd[0].sfd_name,".");
    sd[1].sfd_ino = sd_cwd.sfd_ino;
    strcpy(sd[1].sfd_name,"..");
    for(i = 2; i<8; i++){
        sd[i].sfd_ino = SFS_NOINO;
    }
    disk_write(sd,newbie.sfi_direct[0]);


}

void sfs_rmdir(const char* org_path) 
{
    struct sfs_inode si;
    disk_read( &si, sd_cwd.sfd_ino );

    //for consistency
    assert( si.sfi_type == SFS_TYPE_DIR );

    struct sfs_dir sd[SFS_DENTRYPERBLOCK];
    int c;

    int entryNum = 100;
    int directNum = 0;
    for (c = 0; c < SFS_NDIRECT; c++) {
        disk_read(sd,si.sfi_direct[c]);
        int d;
        for(d=0; d < SFS_DENTRYPERBLOCK;d++) {
            if(strcmp(sd[d].sfd_name,org_path) == 0){
                directNum = c;
                entryNum = d;
                break;
            }
        }
        if(entryNum != 100){
            break;
        }
    }
    // Error4: invalid argument
    if(strcmp(org_path,".") == 0){
        error_message("rmdir",org_path,-8);
        return;
    }
    // Error1: does not exist that dir.
    if(entryNum == 100){
        error_message("rmdir",org_path,-1);
        return;
    }
    struct sfs_inode tnode;
    disk_read(&tnode,sd[entryNum].sfd_ino);
    // Error2 : not a dir
    if(tnode.sfi_type != SFS_TYPE_DIR){
        error_message("rmdir",org_path,-5);
        return;
    }

    // Error3: dir is not empty
    struct sfs_dir tdir[SFS_DENTRYPERBLOCK];
    int z;
    for (z = 0; z < SFS_NDIRECT; z++) {
        disk_read(tdir,tnode.sfi_direct[z]);
        int i;
        for (i = 2; i < SFS_DENTRYPERBLOCK; ++i) {
            if(tdir[i].sfd_ino != 0){
                error_message("rmdir",org_path,-7);
                return;
            }
        }
    }


    disk_read(sd,si.sfi_direct[directNum]);


    int bm[4096]={0,};
    int real[4096]={0,};
    int temp[4096]={0,};
    int afbm[4096];

    int bmNum = 0;
    if(spb.sp_nblocks%4096 == 0){
        bmNum = spb.sp_nblocks/4096;
    }else{
        bmNum = spb.sp_nblocks/4096 + 1;
    }

    int a;
    int curbm = 0;
    int bitFull = 1;
    for(a =2;a < bmNum; a++){
        disk_read(bm,a);
        if(bm[127]!= -1){
            bitFull = 0;
            curbm = a;
            break;
        }
    }

    int x;
    for (x = 0;x <4096 ;x++) {
        afbm[x] = bm[x];
    }

    int p = 0;
    int k = 0;
    int i,j;
    int position;
    int find = 0;
    while(afbm[k] == -1){
        k++;
    }
    while (1){
        temp[p] = bm[k] % 2;
        bm[k] = bm[k] /2;
        p++;
        if(bm[k] ==0)   break;
    }
    for(i = p -1; i >=0; i--) {
        real[p-i-1] = temp[i];
    }

    int flag = 0;
    for (i = 0; i< 512; i++) {
        for (j = 0; j < 8; ++j) {
            if(real[8*i+j] == 0){
                position = 32*k+8*i+j;
                real[8*i+j] = 1;
                flag = 1;
                break;
            }
        }
        if(flag == 1){
            break;
        }
    }

    BIT_CLEAR(afbm[k],sd[entryNum].sfd_ino+1);
    BIT_CLEAR(afbm[k],sd[entryNum].sfd_ino);
    disk_write(afbm,curbm);



    sd[entryNum].sfd_ino = SFS_NOINO;
    si.sfi_size -= sizeof(struct sfs_dir);
    disk_write(sd,si.sfi_direct[directNum]);
    disk_write(&si,sd_cwd.sfd_ino);

}

void sfs_mv(const char* src_name, const char* dst_name) 
{
	struct sfs_inode si;
	disk_read(&si,sd_cwd.sfd_ino);
    assert( si.sfi_type == SFS_TYPE_DIR );

    struct sfs_dir dir[SFS_DENTRYPERBLOCK];
    int i,j;
    for (i = 0; i < SFS_NDIRECT; i++) {
        disk_read(dir,si.sfi_direct[i]);
        for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
            if(strcmp(dir[j].sfd_name,dst_name) == 0){
                error_message("mv",dst_name,-6);
                return;
            }
        }
    }
    int sfind = 0;
    for (i = 0; i < SFS_NDIRECT; i++) {
        disk_read(dir,si.sfi_direct[i]);
        for (j = 0; j < SFS_DENTRYPERBLOCK; j++) {
            if(strcmp(dir[j].sfd_name,src_name) == 0){
                sfind = 1;
                strcpy(dir[j].sfd_name,dst_name);
                disk_write(dir,si.sfi_direct[i]);
                break;
            }
        }
        if(sfind == 1){
            break;
        }
    }

    if(sfind == 0){
        error_message("mv",src_name,-1);
        return;
    }


}

void sfs_rm(const char* path) 
{
    struct sfs_inode si;
    disk_read( &si, sd_cwd.sfd_ino );

    //for consistency
    assert( si.sfi_type == SFS_TYPE_DIR );

    struct sfs_dir sd[SFS_DENTRYPERBLOCK];
    int c;

    int entryNum = 100;
    int directNum = 0;
    for (c = 0; c < SFS_NDIRECT; c++) {
        disk_read(sd,si.sfi_direct[c]);
        int d;
        for(d=0; d < SFS_DENTRYPERBLOCK;d++) {
            if(strcmp(sd[d].sfd_name,path) == 0){
                directNum = c;
                entryNum = d;
                break;
            }
        }
        if(entryNum != 100){
            break;
        }
    }
    // Error1: does not exist that file.
    if(entryNum == 100){
        error_message("rm",path,-1);
        return;
    }
    struct sfs_inode tnode;
    disk_read(&tnode,sd[entryNum].sfd_ino);
    // Error2 : not a dir
    if(tnode.sfi_type == SFS_TYPE_DIR){
        error_message("rm",path,-9);
        return;
    }



    disk_read(sd,si.sfi_direct[directNum]);


    int bm[4096]={0,};
    int real[4096]={0,};
    int temp[4096]={0,};
    int afbm[4096];

    int bmNum = 0;
    if(spb.sp_nblocks%4096 == 0){
        bmNum = spb.sp_nblocks/4096;
    }else{
        bmNum = spb.sp_nblocks/4096 + 1;
    }

    int a;
    int curbm = 0;
    int bitFull = 1;
    for(a =2;a < bmNum; a++){
        disk_read(bm,a);
        if(bm[127]!= -1){
            printf("bm[127]:%d\n",bm[127]);
            bitFull = 0;
            curbm = a;
            break;
        }
    }
    printf("curbm:%d\n",curbm);
    int x;
    for (x = 0;x <4096 ;x++) {
        afbm[x] = bm[x];
    }

    int p = 0;
    int k = 0;
    int i,j;
    int position;
    int find = 0;
    while(afbm[k] == -1){
        k++;
    }
    while (1){
        temp[p] = bm[k] % 2;
        bm[k] = bm[k] /2;
        p++;
        if(bm[k] ==0)   break;
    }
    for(i = p -1; i >=0; i--) {
        real[p-i-1] = temp[i];
    }

    int flag = 0;
    for (i = 0; i< 512; i++) {
        for (j = 0; j < 8; ++j) {
            if(real[8*i+j] == 0){
                position = 32*k+8*i+j;
                real[8*i+j] = 1;
                flag = 1;
                break;
            }
        }
        if(flag == 1){
            break;
        }
    }
    printf("k:%d\n",k);
    BIT_CLEAR(afbm[k],sd[entryNum].sfd_ino);
    int l;
    for (l = 0; l < SFS_NDIRECT; l++) {
        if(tnode.sfi_direct[l] != 0){
            BIT_CLEAR(afbm[k],tnode.sfi_direct[l]);
            tnode.sfi_direct[l] = SFS_NOINO;
        }
    }
    disk_write(&tnode,sd[entryNum].sfd_ino);
    disk_write(afbm,curbm);



    sd[entryNum].sfd_ino = SFS_NOINO;
    si.sfi_size -= sizeof(struct sfs_dir);
    disk_write(sd,si.sfi_direct[directNum]);
    disk_write(&si,sd_cwd.sfd_ino);

}

void sfs_cpin(const char* local_path, const char* path) 
{
	printf("Not Implemented\n");
}

void sfs_cpout(const char* local_path, const char* path) 
{
	printf("Not Implemented\n");
}

void dump_inode(struct sfs_inode inode) {
	int i;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

	printf("size %d type %d direct ", inode.sfi_size, inode.sfi_type);
	for(i=0; i < SFS_NDIRECT; i++) {
		printf(" %d ", inode.sfi_direct[i]);
	}
	printf(" indirect %d",inode.sfi_indirect);
	printf("\n");

	if (inode.sfi_type == SFS_TYPE_DIR) {
		for(i=0; i < SFS_NDIRECT; i++) {
			if (inode.sfi_direct[i] == 0) break;
			disk_read(dir_entry, inode.sfi_direct[i]);
			dump_directory(dir_entry);
		}
	}

}

void dump_directory(struct sfs_dir dir_entry[]) {
	int i;
	struct sfs_inode inode;
	for(i=0; i < SFS_DENTRYPERBLOCK;i++) {
		printf("%d %s\n",dir_entry[i].sfd_ino, dir_entry[i].sfd_name);
		disk_read(&inode,dir_entry[i].sfd_ino);
		if (inode.sfi_type == SFS_TYPE_FILE) {
			printf("\t");
			dump_inode(inode);
		}
	}
}

void sfs_dump() {
	// dump the current directory structure
	struct sfs_inode c_inode;

	disk_read(&c_inode, sd_cwd.sfd_ino);
	printf("cwd inode %d name %s\n",sd_cwd.sfd_ino,sd_cwd.sfd_name);
	dump_inode(c_inode);
	printf("\n");

}
