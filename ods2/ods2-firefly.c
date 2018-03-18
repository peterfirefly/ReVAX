/* Fugly old hacky code to read ODS2 Files-11 disk images (VMS).

   The original code is from around 1982 but based on an earlier program that
   read ODS-1 images.

   Somewhat modernized in order to remove warnings and improve readability.
   VMS code removed.

   ./a.out '-n xxx.iso'

   The command line parser is completely fucked up.  It assumes that -n and
   the image name following it are part of the same argument string.  It also
   stops at the first '-' in image names.

   -l '-n xxx.iso' '[000000]'     		list main directory
   -l '-n xxx.iso' '[000000.DOCUMENTATION]'     list DOCUMENTATION directory
   -l '-n xxx.iso' '[DOCUMENTATION]'		list DOCUMENTATION directory
      '-n xxx.iso' '[DOCUMENTATION.V071]OVMS_71_VAX_INSTALL.TXT;1' extract a file

 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>


/* a-zA-Z0-9-_$ */
#define alphnum(x)	(isalnum(x) || ((x) == '-') || ((x) == '_') || ((x) == '$'))


struct filnam {
/*	char	f_nam[14]; */	/* File name (ASCII) */
	char	f_nam[80];	/* File name (ASCII), int32_t enough for V4 on */
	uint16_t	f_ver;		/* Version number */
};

struct uic {
	uint16_t	u_prog; 	/* Programmer number */
	uint16_t	u_proj; 	/* Project number */
};

struct fileid {
	uint16_t	f_num;		/* File number */
	uint16_t	f_seq;		/* File sequence number (worthless concept) */
	uint16_t	f_rvn;		/* Relative volume number (ditto and MBZ) */
};

struct rms {
	char	f_forg; 	/* Record format and file organization */
	char	f_ratt; 	/* Record attributes */
	uint16_t	f_rsiz; 	/* Record size */
	uint16_t	f_hvbn[2];	/* Highest VBN allocated */
	uint16_t	f_heof[2];	/* End of file block */
	uint16_t	f_ffby; 	/* First free byte */
	char	f_bksz; 	/* Bucket size */
	char	f_hdsz; 	/* Fixed header size */
	uint16_t	f_mrs;		/* Maximum record size */
	uint16_t	f_deq;		/* Default extend quantity */
};

struct ident {
	char	i_fnam[20];	/* File name */
	uint16_t	i_rvno; 	/* Revision number */
	char	i_crdt[8];	/* Creation date and time */
	char	i_rvdt[8];	/* Revision date and time */
	char	i_exdt[8];	/* Expiration date and time */
	char	i_bkdt[8];	/* Backup date and time */
	char	i_ulab[80];	/* User label */
};

struct header {
	char	h_idof; 	/* Ident area offset */
	char	h_mpof; 	/* Map area offset */
	char	h_acof; 	/* Access control list offset */
	char	h_rsof; 	/* Reserved area offset */
	uint16_t	h_fseg; 	/* Extension segment number */
	uint16_t	h_flev; 	/* Structure level and version */
	uint16_t	h_fnum; 	/* File number */
	uint16_t	h_fseq; 	/* File sequence number */
	uint16_t	h_frvn; 	/* Relative volume number */
	uint16_t	h_efnu; 	/* Extension file number */
	uint16_t	h_efsq; 	/* Extension file sequence number */
	uint16_t	h_ervn; 	/* Extension relative volume number */
	union {
		char	hu_ufat[32];	/* User file attributes */
		struct rms hu_rms;	/* RMS file attributes */
	} h_ufat;
#define h_rms	h_ufat.hu_rms
	char	h_fcha[4];	/* File characteristics */
#define h_ucha	h_fcha[0]	/* User controlled characteristics */
#define h_scha	h_fcha[1]	/* System controlled characteristics */
	char	h_UU1[2];	/* Unused 1 */
	char	h_use;		/* Map words in use */
	char	h_priv; 	/* Accessor privilege level */
	struct uic h_fown;	/* File owner UIC */
#define h_prog	h_fown.u_prog	/* Programmer (member) number */
#define h_proj	h_fown.u_proj	/* Project (group) number */
	uint16_t	h_fpro; 	/* File protection code */
	uint16_t	h_rpro; 	/* Record protection code */
	char	h_UU2[4];	/* Ununsed 2 */
	char	h_semk[4];	/* Security mask */
	struct ident h_ident;	/* Ident area */
	char	h_other[300];	/* Map area, access control area, etc */
};

struct homeblock {
	int32_t	H_hblb; 	/* Home block LBN */
	int32_t	H_ahlb; 	/* Alternate home block LBN */
	int32_t	H_ihlb; 	/* Backup index file header LBN */
	char	H_vlev[2];	/* Structure level and version */
	uint16_t	H_sbcl; 	/* Storage bitmap cluster factor */
	uint16_t	H_hbvb; 	/* Home block VBN */
	uint16_t	H_ahvb; 	/* Backup home block VBN */
	uint16_t	H_ihvb; 	/* Backup index file header VBN */
	uint16_t	H_ibvb; 	/* Index file bitmap VBN */
	uint16_t	H_iblb[2];	/* Index file bitmap LBN */
	int32_t	H_fmax; 	/* Maximum number of files */
	uint16_t	H_ibsz; 	/* Index file bitmap size */
	uint16_t	H_rsvf; 	/* Number of reserved files */
	uint16_t	H_dvty; 	/* Disk device type */
	uint16_t	H_rvn;		/* Relative volume number */
	uint16_t	H_nvol; 	/* Number of volumes */
	uint16_t	H_vcha; 	/* Volume characteristics */
	struct uic H_vown;	/* Volume owner UIC */
	int32_t	H_vsmx; 	/* Volume security mask */
	uint16_t	H_vpro; 	/* Volume protection code */
	uint16_t	H_dfpr; 	/* Default file protection */
	uint16_t	H_drpr; 	/* Default record protection */
	uint16_t	H_chk1; 	/* First checksum */
	char	H_vdat[8];	/* Volume creation date */
	char	H_wisz; 	/* Default window size */
	char	H_lruc; 	/* Directory pre-access limit */
	uint16_t	H_fiex; 	/* Default file extend */
	char	H_UU1[388];	/* Unused 1 */
	char	H_snam[12];	/* Structure name */
	char	H_indn[12];	/* Volume name */
	char	H_indo[12];	/* Volume owner */
	char	H_indf[12];	/* Format type */
	char	H_UU2[2];	/* Unused 2 */
	uint16_t	H_chk2; 	/* Second checksum */
} hblock;

struct directory {
	uint16_t	d_rbc;		/* Record byte count */
	uint16_t	d_vrlm; 	/* Version limit */
	char	d_flags;	/* Flags */
	char	d_nbc;		/* Name byte count */
	char	d_fname[1];	/* File name string */
};

struct dirval {
	uint16_t	d_ver;		/* Version number */
	struct fileid d_fid;	/* File ID */
};


#define BUFSIZE 512

#define bit(x)	((01)<<(x))

#define DEV	bit(0)
#define DIR	bit(1)
#define FIL	bit(2)
#define EXT	bit(3)
#define VER	bit(4)

#define DIRBEG	'['
#define DIREND	']'

#define FSMAX	250
#define DEVMAX	20

#define TEXT	0
#define IMGRMS	1
#define IMGFULL 2
#define BINARY	3

#define DISK	0
#define STDOUT	1

#define DFLTMOD TEXT
#define DFLTOUT DISK


struct cracked {
	char	*dev, *dir, *fil, *typ, *ver;	/* Pointers to cracked filename fields */
};

/* active during traversal of disk image */
int	vmsfd = -1;			/* File descriptor for reading VMS filesystem */
FILE	*of;				/* Stream pointer for output file */

struct header	indexh, mfdh, dirh, fileh;	/* File headers for index file, MFD,
 UFD, and file */


/***/

char	**current_fname;	/* for error reporting only */

void errmsg(char *msg, ...)
{
	va_list arglist;

	fprintf(stderr, "%s -- ", *current_fname);
	va_start(arglist, msg);
	vfprintf(stderr, msg, arglist);
	va_end(arglist);
	fprintf(stderr, "\n");
}

/* ## before __VA_ARGS__ is a GNU extension */
#define err_ret0(msg, ...)	do {					\
					errmsg(msg, ## __VA_ARGS__);	\
					return 0;			\
				} while (0)

/***/


/*
 *  Convert file name, type, and version number to "struct filnam" format
 */

int convert(char *fl, char *tp, char *vr, struct filnam *f)
{
	char *p;

	if (strlen(fl) > 39)
		err_ret0("Filename longer than 39 characters");
	if (strlen(tp) > 39)
		err_ret0("File type longer than 39 characters");
	strcpy(f->f_nam, fl);
	strcat(f->f_nam, ".");
	strcat(f->f_nam, tp);
	for (p=f->f_nam; *p; ++p )		/* This code is needed since */
		*p = toupper(*p);		/* VMS loves to SHOUT at you */
						/* in UPPER CASE all the time */
	for (f->f_ver=0; *vr;) {
		if (!isdigit(*vr))
			err_ret0("Non-digit in version number");
		f->f_ver *= 10;
		f->f_ver += *vr++ - '0';
	}
	return 1;
}




/*
 *  Process next character from input file
 *  for text mode
 */

/*
 * possible states of the machine:
 */

#define INIT	0	/* waiting for the beginning of a record */
#define COUNT	1	/* in byte count */
#define LINENO	2	/* in line number */
#define DATA	3	/* in data */
#define NULLPAD	4	/* eating the padding null at the end */

int	rmlineno = 0; /* FIXME can we kill it entirely? */

void putch(char c)
{
	static unsigned	count;
	static int	state = INIT;
	static int	nextstate;
	static int	lnbytes;

	switch (state) {
	case INIT:
		count = c & 0xFF;
		state = COUNT;
		break;

	case COUNT:
		if ((count+=((c & 0xFF)<<8)) == 0) {
			putc('\n', of);
			state = INIT;
		} else {
			if (rmlineno == 0)
				state = DATA;
			else {
				lnbytes = 0;
				state = LINENO;
			}
			nextstate = INIT;
			if (count&1)
				nextstate = NULLPAD;
		}
		break;

	case LINENO:
		if (lnbytes == 0)
			lnbytes++;
		else
			state = DATA;
		if (--count == 0) {
			putc('\n', of);
			state = INIT;
		}
		break;

	case DATA:
		putc(c, of);
		if (--count == 0) {
			state = nextstate;
			putc('\n', of);
		}
		break;

	case NULLPAD:
		state = INIT;
		break;

	default:
		errmsg("internal error in putch");
		abort();
	}
}


/***/

/* Low-level stuff */

#define WTPMASK 0xC000
#define WTP00	0x0000
#define WTP01	0x4000
#define WTP10	0x8000
#define WTP11	0xC000

/*
 *  Return base lbn mapped by the current window
 */

int32_t lbnbase(uint16_t *rp)
{
	uint16_t wtype;

	wtype = (*rp) & WTPMASK;
	switch (wtype) {
		case WTP00:	return 0L;
		case WTP01:	return ((((char *)rp)[1]&0x3FL)<<16)+rp[1];
		case WTP10:	return (((int32_t)rp[2])<<16)+(int32_t)rp[1];
		case WTP11:	return (((int32_t)rp[3])<<16)+(int32_t)rp[2];
	}

	assert(0);
}


/*
 *  Return number of blocks mapped by the current window
 */

uint16_t getsize(uint16_t *rp)
{
	uint16_t	wtype;

	wtype = (*rp) & WTPMASK;
	switch (wtype) {
		case WTP00:	return 0;
		case WTP01:	return ((*((char *)rp))&0xFF)+1;
		case WTP10:	return ((*rp)&0x3FFF)+1;		
		case WTP11:	return ((((int32_t)(*rp)&0x3FFF)<<16)+rp[1])+1;
	}

	assert(0);	/* kill a warning */
}


/*
 *  Get block from the filesystem, given the logical block number
 */

int getlb(int32_t lbn, char *buf)
{
	if (lbn == 0L)
		err_ret0("Bad block in file");
	if (lseek(vmsfd, BUFSIZE*lbn, 0) == -1L ||  read(vmsfd, buf, BUFSIZE) != BUFSIZE)
		err_ret0("Read error");
	return 1;
}


/*
 *  Routine to get specified virtual block from a file.  Returns 0
 *  on EOF, 1 otherwise.  Note that vbn is 1-based, not 0-based.
 */

int getvb(int32_t vbn, char *buf, struct header *hp)
{
	uint16_t 		*rp;
	int32_t		 block;
	uint16_t 		*limit;
	uint16_t 		 wtype;
	int32_t		 lbn;
	int32_t		 size;

	rp = (uint16_t *)hp + (hp->h_mpof&0xFF);
	block = 1;
	limit = rp + (hp->h_use & 0xFF);		/* ntw */
	while (rp < limit && vbn >= (block + (size=getsize(rp)))) {
		wtype = (*rp) & WTPMASK;
		switch (wtype) {
			case WTP00:	rp += 1; break;
			case WTP01:	rp += 2; break;
			case WTP10:	rp += 3; break;
			case WTP11:	rp += 4; break;
		}
		block += size;
	}
	if (rp >= limit)
		return 0;
	lbn = lbnbase(rp) + vbn - block;
	return getlb(lbn, buf);
}


/*
 *  Get a file header, given the file number; check access privilege
 */

int gethdr(uint16_t fnum, struct header *hp)
{
#define G_DENY	bit(8)
#define W_DENY	bit(12)
	int32_t	bn;
	int		grp;
	int		ogrp;

	bn = (int32_t)fnum + hblock.H_ibvb + hblock.H_ibsz -1;
	if (!getvb(bn, (char *) hp, &indexh))
		return 0;

/* FIXME getgid() ?!   erase all this privilege stuff? */

/* dyke out priv. checks here for now. */
/*
	if (!(hp->h_fpro&W_DENY) || !getuid())
		return 1);
*/
	grp = getgid();
	ogrp = 64*(grp/100) + 8*((grp/10)%10) + (grp%10);
	(void) ogrp; /* kill a warning */

	return 1; /* dyke priv checks */
/*
	if (ogrp != hp->h_proj || hp->h_fpro&G_DENY)
		return -1);
	else
		return 1);
*/
}



/*
 *  Return pointer to next directory entry
 */

struct directory *getde(struct header *dhp, int bod)
{
#define recsize (*((uint16_t *)de))
#define STOP	((uint16_t) 0xFFFF)
	static int32_t		vb;
	static int32_t		eofblk;
	static char		*limit;
	static char		dirbuf[BUFSIZE];
	static char		*de;

	if (bod) {
		vb = 0;
		eofblk = ((int32_t)dhp->h_rms.f_heof[0] << 16) + dhp->h_rms.f_heof[1];
		limit = &dirbuf[BUFSIZE];
	}
	if (bod || (de+=(recsize+2))>=limit || recsize==STOP) {
		if (++vb == eofblk )
			limit = &dirbuf[dhp->h_rms.f_ffby];
		if (!getvb(vb, dirbuf, dhp) || (*((uint16_t *)dirbuf)) == STOP)
			return NULL;
		de = dirbuf;
	}
	if (de >= limit)
		return NULL;
	return (struct directory *) de;
}


/***/

/*
 *  Search a directory (identified by dhp) for a filename
 */

uint16_t search(struct header *dhp, struct filnam *fn)
{
	int			 len;
	int			 bod;
	struct directory	*de;
	struct directory	*getde();
	struct dirval		*vp;
	struct dirval		*vplim;

fprintf(stdout, "fn->f_nam '%s'\n", fn->f_nam);
fprintf(stdout, "fn->f_ver %d\n", fn->f_ver);

	len = strlen(fn->f_nam);
	/* FIXME dafuq?!  bod=1 for first getde() call, bod=0 for the rest? */
	for (bod=1; (de=getde(dhp, bod)); bod=0) {
		if (de->d_nbc!=len || strncmp(de->d_fname, fn->f_nam, len)!=0)
			continue;
		vp = (struct dirval *) (de->d_fname + ((de->d_nbc+1) & 0xFE));
		if (!fn->f_ver)
			return vp->d_fid.f_num;
		for (vplim=(struct dirval *)((char *)(&de->d_vrlm)+de->d_rbc);
		     vp<vplim;
		     ++vp) {
			if (vp->d_ver > fn->f_ver)
				continue;
			if (vp->d_ver == fn->f_ver)
				return vp->d_fid.f_num;
			return 0;
		}
		return 0;
	}
	return 0;
}


/*
 *  Error accessing a directory
 */

void dirmsg(char *msg, char *dirname, char *ptr)
{
	char	c;

	/* FIXME  dafuq? */
	c = *ptr;
	*ptr = '\0';
	errmsg(msg, dirname);
	*ptr = c;
}



/*
 *  Locate the directory whose name is pointed to by "dir"
 */

int finddir(char *dir, int *dirfound)
{
#define direrr(msg, dirname, ptr) { dirmsg(msg, dirname, ptr); return 0; }
	struct header	*hp = &mfdh;
	char		*p = dir;
	char		*q;
	int		nch;
	struct filnam	dirfn;
	uint16_t	dirfnum;
	int		gh;

	do {
		for (q=p; alphnum(*q); ++q)
			;
		if ((*q && *q != '.') || (nch=q-p) == 0 || nch > 39)
			err_ret0("Invalid directory ([%s])", dir);
		strncpy(dirfn.f_nam, p, nch);
		dirfn.f_nam[nch] = '\0';
		strcat(dirfn.f_nam, ".DIR");
		dirfn.f_ver = 1;
		if (!(dirfnum=search(hp, &dirfn)))
			direrr("Directory [%s] does not exist", dir, q);
		if (!(gh=gethdr(dirfnum, (hp=(&dirh)))))
			direrr("Can't get file header for directory [%s]", dir, q);
		if (gh == -1)
			direrr("No access privilege for directory [%s]", dir, q);
		p = q + 1;
	} while (*q);
	*dirfound = 1;
	return 1;
}


/***/


/*
 *  Write filename to standard output
 */

void prtfn(struct directory *de, struct dirval *vp)
{
	char	*p;
	int	 i;

	for (p=de->d_fname, i=de->d_nbc; i>0; --i)
		putc(*p++, stdout);
	fprintf(stdout, ";%d\n", vp->d_ver);
}


/*
 *  List contents of a UFD
 */

void listdir(void)
{
	struct directory	*de;
	struct dirval		*vp;
	struct dirval		*vplim;

#if 0
	int	bod = 1;
	while ((de = getde(&dirh, bod))) {
		vp = (struct dirval *) (de->d_fname + ((de->d_nbc+1) & 0xFE));
		vplim = (struct dirval *) ((char *)(&de->d_vrlm)+de->d_rbc);
		for (; vp<vplim; ++vp)
			prtfn(de, vp);

		bod = 0;
	}
#else
	int	bod;
	for (bod=1; (de=getde(&dirh, bod)); bod=0) {
		vp = (struct dirval *) (de->d_fname + ((de->d_nbc+1)&0xFE));
		vplim = (struct dirval *) ((char *)(&de->d_vrlm)+de->d_rbc);
		for (; vp<vplim; ++vp)
			prtfn(de, vp);
	}
#endif
}


/***/

/*
 *  Copy input file to output destination
 */

int copyfile(int xfermode)
{
	int32_t		 eofblk;
	int32_t		 block = 0;
	int32_t		 b = 0;
	char		 buf[BUFSIZE];
	size_t		 nbytes = BUFSIZE;
	char		*p;

	if (xfermode == BINARY)
		err_ret0("Binary mode not yet supported");
	if (xfermode != IMGFULL)
		eofblk = ((int32_t) fileh.h_rms.f_heof[0] << 16) + fileh.h_rms.f_heof[1];

	while (getvb(++block, buf, &fileh)) {
		if (xfermode == IMGFULL) {
			if (fwrite(buf, 1, BUFSIZE, of) == BUFSIZE)
				continue;
			err_ret0("write error");
		}
		if (++b > eofblk)
			return 1;
		if (b == eofblk)
			nbytes = fileh.h_rms.f_ffby;
		if (xfermode == IMGRMS) {
			if (fwrite(buf, 1, nbytes, of) == nbytes)
				continue;
			err_ret0("write error");
		}
		for (p=buf; p<buf+nbytes;)
			putch(*p++);
	}
	return 1;
}



/***/

/*
 *  Open a disk containing an VMS filesystem
 */

int openvms(char *devname)
{
	char		vmsdev[DEVMAX+6];	/* Special file name for VMS filesystem */
	int32_t		ifhbn;


	if (strlen(devname) > DEVMAX)
		err_ret0("Device name too long (%s)", devname);
	strcpy(vmsdev, "");
	strcat(vmsdev, devname);
        vmsfd = open(vmsdev, O_RDONLY);
	if (vmsfd < 0)
		err_ret0("Can't open %s", vmsdev);

	if (!getlb(1L, (char *) &hblock))
		err_ret0("Can't read homeblock on %s", vmsdev);

	ifhbn = ((int32_t)hblock.H_iblb[1]<<16) + (int32_t)hblock.H_iblb[0] + hblock.H_ibsz;
	if (!getlb(ifhbn, (char *) &indexh))
		err_ret0("Can't read index file header on %s\n", vmsdev);

	if (!getlb(ifhbn+3, (char *) &mfdh))
		err_ret0("Can't read mfd header on %s", vmsdev);

	return 1;
}


/*
 *  Crack the filename string -- First step in parsing it; just
 *  locates the fields, doesn't do much real validity checking
 */

int crack(char *filspec, int *pflags, struct cracked *cracked)
{
	char	*p = filspec;
	char	*q;

	cracked->dev = "";
	cracked->dir = "";
	cracked->fil = "";
	cracked->typ = "";
	cracked->ver = "";

	for (*pflags=0; *p;) {

		if (*p == DIRBEG) {
			if (*pflags & (DIR|FIL|EXT|VER))
				err_ret0("Bad filename syntax");
			cracked->dir = p+1;
			while (*p != DIREND) {
				*p = toupper(*p);	/* SHOUT the directory */
							/* name in UPPER CASE */
				if (*p++ == '\0')
					err_ret0("Bad filename syntax");
			}
			*p++ = '\0';
			*pflags |= DIR;
			continue;
		}

		for (q=p; alphnum(*q); ++q)
			;

		if (*q == ':') {
			if (*pflags & (DEV|DIR|FIL|EXT|VER))
				err_ret0("Bad filename syntax");
			cracked->dev = p;
			*pflags |= DEV;
			*q = '\0';
			p = q + 1;
			continue;
		}

		if (*q == '.' || *q == ';' || *q == '\0') {

			if (!(*pflags & FIL)) {
				if (p == q)
					err_ret0("Filename missing");
				cracked->fil = p;
				*pflags |= FIL;
				if (*q == ';') {
					cracked->typ = "";
					*pflags |= EXT;
				}
			} else if (!(*pflags & EXT)) {
				cracked->typ = p;
				*pflags |= EXT;
			} else if (!(*pflags & VER)) {
				cracked->ver = p;
				*pflags |= VER;
			} else
				err_ret0("Bad filename syntax");

			if (*q == '\0') {
				if (!(*pflags & EXT))
					cracked->typ = "";
				if (!(*pflags & VER))
					cracked->ver = "";
				break;
			}
			*q = '\0';
			p = q + 1;
			continue;
		}

		err_ret0("Bad filename syntax");
	}

fprintf(stdout, " dev: '%s'\n", cracked->dev);
fprintf(stdout, " dir: '%s'\n", cracked->dir);
fprintf(stdout, " fil: '%s'\n", cracked->fil);
fprintf(stdout, " typ: '%s'\n", cracked->typ);
fprintf(stdout, " ver: '%s'\n", cracked->ver);
	return 1;
}


/*
 *  Open VMS file for input
 */

int openin(char *nativdev, int lsflag, char *filspec, char *outfname)
{
	static int	filecnt = 0;
	int		dirfound= 0;
	struct filnam	fn;
	uint16_t	fnum;
	int		gh;
	int		pflags;
	struct cracked	cracked;

	++filecnt;
	if (crack(filspec, &pflags, &cracked) == 0)
		return 0;
	if (pflags & DEV && !openvms(cracked.dev))
		return 0;

        if (strlen(nativdev) > 0 && !(pflags & DEV) && filecnt==1 && !openvms(nativdev))
		return 0;
	if (vmsfd < 0)
		err_ret0("No device specified");
	if (pflags & (DEV|DIR) && !finddir(cracked.dir, &dirfound))
		return 0;
	if (!dirfound)
		err_ret0("No directory specified");
	if (lsflag) {
		if (pflags & (FIL|EXT|VER))
			err_ret0("Invalid directory specification");
		return 1;
	}
	if (!(pflags & EXT))
		cracked.typ = "";
	if (!(pflags & VER))
		cracked.ver = "";
	if (!convert(cracked.fil, cracked.typ, cracked.ver, &fn))
		return 0;
	if (!(fnum=search(&dirh, &fn)))
		err_ret0("File does not exist");
	if (!(gh=gethdr(fnum, &fileh)))
		err_ret0("Can't get file header for file");
	if (gh == -1)
		err_ret0("No access privilege for file");

	strcpy(outfname, cracked.fil);
	strcat(outfname, ".");
	strcat(outfname, cracked.typ);

	return 1;
}



/***/


/*
 * see if ok to write/create this file
 * needed because we might be setuid or setgid
 * to get at the special files for disks
 * nb we assume the file is in the working directory
 * always true at the moment;  might neeed more mess in future
 */

int okwrite(const char *file)
{

	if (access(file, W_OK) == 0)
		return 1;		/* exists and is writeable */
	if (access(file, F_OK) == 0)
		return 0;		/* exists although not writeable */
	if (strchr(file, '/'))
		return 0;		/* snh */

	if (access(".", W_OK) == 0)
		return 1;		/* file doesn't exist and can create it */
	return 0;
}


/*
 *  Open output file
 */

int openout(int outdest, const char *fname)
{
	if (outdest == STDOUT) {
		of = stdout;
		return 1;
	}

	if (okwrite(fname) == 0 || (of=fopen(fname, "w")) == NULL)
		err_ret0("Can't open output file");
	return 1;
}


/***/


/*
 *  Get the next requested file from the VMS filesystem
 */

void getvms(char *nativdev, char *fname, int lsflag, int outdest, int xfermode)
{
	char	filspec[FSMAX]; 		/* Full filename string being processed */
	char	outfname[256];

fprintf(stdout, "getvms('%s', '%s', %d, %d, %d)\n",
	nativdev, fname, lsflag, outdest, xfermode);

	if (strlen(fname) > FSMAX) {
		errmsg("Filespec too long");
		return;
	}
	strcpy(filspec, fname);

	if (lsflag) {
		if (openin(nativdev, lsflag, filspec, outfname))
			listdir();
	} else {
		if (openin(nativdev, lsflag, filspec, outfname) && openout(outdest, outfname)) {
			copyfile(xfermode);
			if (of != stdout)
				fclose(of);
		}
	}
}


/***/

/* Command line parsing */

/* command line */
char	**av;				/* Global argv */
char	lsflag = 0;			/* Nonzero ==> list directory */
int	xfermode = DFLTMOD;		/* Transfer mode */
int	outdest = DFLTOUT;		/* Output destination */

/* FIXME */
#define NDEVMAX 256
char	nativdev[NDEVMAX];
int	natlen;



void options()
{
	char	*p;

	for (p = *av; *++p;) {
		switch (*p) {
		case 'd':
		case 'f':	outdest = DISK; break;
		case 's':	outdest = STDOUT; break;

		case 't':	xfermode = TEXT; break;
		case 'i':	xfermode = IMGRMS; break;
		case 'I':	xfermode = IMGFULL; break;
		case 'b':	xfermode = BINARY; break;
		case 'n':	{
				int	 kk;
				char	*cp;
				char	 cac;
//				char	*cop;

//				cop = &nativdev[0];
				kk = 0;
		                natlen=0;
		                cp = p;

		                cac = *++cp;
				if (cac == ' ') cac=*++cp;
				if (cac == ' ') cac=*++cp;
				if (cac == ' ') cac=*++cp;
				if (cac == ' ') cac=*++cp;
				if (cac == ' ') cac=*++cp;
				if (cac == ' ') cac=*++cp;
				if (cac == ' ') cac=*++cp;
				if (cac == ' ') cac=*++cp;

				while (*cp != '\0' && *cp != ' ' && *cp != '-'){
				if (cac != ' ' && kk < NDEVMAX-3 && cac != '\0'){
				  nativdev[kk]=cac;
				  nativdev[kk+1] = '\0';
				  kk++;
				  natlen++;
				}
				cac = *++cp;
				p = cp;
				}
				nativdev[kk] = '\0';
                                nativdev[kk+1] = '\0';
		                nativdev[kk+2] = '\0';
		                p = &nativdev[kk];
				break;
				}
		                break;  
		case 'l':	++lsflag; break;
		case 'c':	lsflag = 0; break;

		default:	fprintf(stderr, "Invalid option (%c)\n", *p);
		}
	}
}


void usage(void)
{
	/* FIXME way out of date! */
	fprintf(stderr, "usage: %s [-t][-n nativdev][-i][-b][-d][-f][-s] vmsfile\n", *av);
	exit(-1);
}


char *basename(char *s)
{
	char	*t;

	if ((t=strrchr(s, '/')) == NULL)
		return s;
	else
		return t+1;
}


#if 0
int main(int argc, char **argv)
{
	(void) argc, (void) argv;

	current_fname = (char *[]) {"README.TXT"};
	getvms(/* nativdev   */ "xxx.iso",
	       /* av/vmsfile */ "README.TXT",
	       /* lsflag     */ 1,
	       /* outdest    */ DFLTOUT,
	       /* xfermode   */ DFLTMOD);
}
#else
int main(int argc, char **argv)
{
	av = argv;

	if (--argc == 0)
		usage();

	/* FIXME dafuq?! */
	if (*basename(*argv) == 'l')
		++lsflag;

	natlen = 0;
	while (argc--) {
		if (**++av == '-')
			options();
		else {
			current_fname = av;
			getvms(nativdev, *av, lsflag, outdest, xfermode);
		}
	}
}
#endif

