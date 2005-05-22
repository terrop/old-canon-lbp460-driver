/*
 *  Printer driver for Canon LBP-660 laser printer
 *  Copyright (C) 2004 Nicolas Boichat <nicolas@boichat.ch>
 *
 *  Adapted from a printer driver for Samsung ML-85G laser printer
 *  (C) Copyleft, 2000 Rildo Pragana <rpragana@acm.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/io.h> /* for outb() and inb() */
#include <sys/time.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* needed for ioperm() */

#include "lbp660.h"

static struct printer
{
	const char *name;
	int lines_by_page;
} printers[] = {
	{
		.name = "LBP-460",
		.lines_by_page = 3484,
	}, {
		.name = "LBP-660",
		.lines_by_page = 6968,
	}, {
		NULL
	}
};

static int pagedata[] =
{
	-1, 0x89, /*100*/
	-1, 0x8a, /*172*/
	-1, 0x8b, /*244*/
	-3, 0x8b, 0x89, 0x8c, /*333*/
	-7, 0x8c, 0x4, 0x94, 0x3f, 0x95, 0x58, 0x94, /*456*/
	-1, 0x95, /*528*/
	-1, 0x89, /*600*/
	-1, 0x8a, /*672*/
	-1, 0x8b, /*744*/
	-1, 0x89, /*816*/
	-1, 0x8a, /*888*/
	-1, 0x8b, /*960*/
	-3, 0x8b, 0x89, 0x90, /*1049*/
	-5, 0x91, 0x0, 0x90, 0x0, 0x89, /*1155*/
	-1, 0x8a, /*1227*/
	-1, 0x8b, /*1299*/
	-1, 0x89, /*1372*/
	-1, 0x8a, /*1444*/
	-3, 0x8d, 0xa7, 0x8b, /*1533*/
	-3, 0x8b, 0x8a, 0x90, /*1622*/
	-3, 0x90, 0x8, 0x89, /*1712*/
	-1, 0x8a, /*1784*/
	-1, 0x8e, /*1856*/
	-1, 0x90, /*1928*/
	-3, 0x90, 0x9, 0x89, /*2018*/
	-1, 0x8a, /*2090*/
	-3, 0x8d, 0x40, 0x90, /*2179*/
	-5, 0x90, 0xc9, 0xa0, 0x0, 0xa0, /*2285*/
	-17, 0x81, 0xdc, 0x82, 0x0, 0x83, 0x61, 0x84, 0x0, 0x85,
	0x58, 0x86, 0x2, 0x87, 0x9c, 0x88, 0x1a, 0x81, /*2493*/
	-1, 0x82, /*2565*/
	-1, 0x83, /*2637*/
	-1, 0x84, /*2709*/
	-1, 0x85, /*2781*/
	-1, 0x86, /*2853*/
	-1, 0x87, /*2925*/
	-1, 0x88, /*2997*/
	-1, 0x89, /*3070*/
	-1, 0x8a, /*3142*/
	-1, 0x8e, /*3214*/
	-1, 0x89, /*3287*/
	-1, 0x8a, /*3359*/
	-3, 0x8d, 0x2, 0x89, /*3449*/
	-1, 0x8a, /*3521*/
	-1, 0x8e, /*3593*/
	-1, 0x89, /*3666*/
	-1, 0x8a, /*3738*/
	-3, 0x8d, 0x9d, 0x89, /*3828*/
	-1, 0x8a, /*3900*/
	-1, 0x8e, /*3972*/
	-1, 0x89, /*4045*/
	-1, 0x8a, /*4117*/
	-3, 0x8d, 0x2, 0x89, /*4207*/
	-1, 0x8a, /*4279*/
	-1, 0x8e, /*4351*/
	-3, 0x93, 0x0, 0x89, /*4441*/
	-1, 0x8a, /*4513*/
	-3, 0x8d, 0x2, 0x89, /*4603*/
	-1, 0x8a, /*4675*/
	-1, 0x8e, /*4747*/
	-1, 0x89, /*4820*/
	-1, 0x8a, /*4892*/
	-1, 0x89, /*4965*/
	-1, 0x8a,
	-256,
	-260 }; //, /*5037*/

static int bandinit[] =
{
	-1, 0x8a, /*25157*/
	-1, 0x8e, /*25229*/
	-1, 0x89, /*25302*/
	-1, 0x8a, /*25374*/
	-1, 0x89, /*25447*/
	-1, 0x8a, /*25519*/
	-3, 0x8d, 0x1, 0x89, /*25609*/
	-1, 0x8a, /*25681*/
	-1, 0x89, /*25754*/
	-1, 0x8a, /*25826*/
	//-3, 0x80, 0xff, 0x7f, /*25909*/
	-256, /* data */
	-260
};

void INLINE errorexit();

static int lines_by_page;

/* Rildo Pragana constants and functions */
static FILE *cbmf = NULL;
static int bmcnt = 0;
static unsigned char bmbuf[800]; 	/* the pbm bitmap line with provision for leftskip */
static unsigned char *bmptr = bmbuf;
static int bmwidth = 0, bmheight = 0;
static unsigned char cbm[300000];	/* the compressed bitmap */
static unsigned char garbage[600];
static unsigned char *cbmp = cbm;
static int csize = 0;				/* compressed line size */
static int linecnt = 0;
static int pktcnt;
static int topskip = 0;
static int leftskip = 0;

static void message (char *fmt, ...)
{
	va_list args;
	va_start (args, fmt);
	vfprintf (stderr, fmt, args);
	va_end (args);
}


static void bitmap_seek (FILE *bitmapf, int offset)
{
	if (offset)
	{
		while (offset > sizeof(garbage))
		{
			fread(bmbuf,1,sizeof(garbage),bitmapf);
			offset -= sizeof(garbage);
		}
		fread(bmbuf,1,offset,bitmapf);
	}
}

static unsigned char get_bitmap (FILE *bitmapf)
{
	if (bmcnt == 0)
	{
		memset (bmbuf, 0, 800);
		if (linecnt < (bmheight - topskip))
		{
			if (bmwidth > 800)
			{
				fread (bmbuf, 1, 800, bitmapf);
				bitmap_seek (bitmapf, bmwidth - 800);
			} else {
				fread (bmbuf, 1, bmwidth, bitmapf);
			}
		}
		bmptr = bmbuf + leftskip / 8;
		bmcnt = LINE_SIZE;
		linecnt++;
	}
	bmcnt--;
	return *bmptr++;
}

static void next_page (FILE *bitmapf, int page)
{
	/* we can't use fseek here because it may come from a pipe! */
	int skip;
	skip = (bmheight - topskip - linecnt) * bmwidth;
	message ("bmheight = %d, bmwidth = %d, leftskip = %d, "
		 "topskip = %d, linecnt = %d, skip = %d\n",
		 bmheight, bmwidth, leftskip, topskip, linecnt, skip);
	if (skip > 0)
		bitmap_seek (bitmapf, skip);
	linecnt = 0;
}

static void out_packet (int rle, unsigned char a, unsigned char b, unsigned char c)
{
	static unsigned char parity[] =
	{
		0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
		1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
		1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
		0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0
	};

	union pkt1 pk1;
	union pkt2 pk2;
	union pkt3 pk3;
	union pkt4 pk4;
	int tmp;

	if (rle == 2)
	{ // flush packet storage
		if (a == 1)
		{ // truncated band
			tmp = csize | 0x10000;
			fwrite (&tmp, 1, sizeof (int), cbmf);
		} else {
			fwrite (&csize, 1, sizeof (int), cbmf);
		}

		fwrite (cbm, csize, 4, cbmf);
		cbmp = cbm;
		csize = 0;
		return;
	}

	pk1.bits.t = 0;
	pk2.bits.t = 1;
	pk3.bits.t = 0;
	pk4.bits.t = 1;
	pk1.bits.rle = rle;
	pk1.bits.al = a & 0x3f;
	pk2.bits.ah = (a >> 6) & 0x3;
	pk2.bits.bl = b & 0xf;
	pk3.bits.bh = (b >> 4) & 0xf;
	pk3.bits.cl = c & 0x3;
	pk4.bits.ch = (c >> 2) & 0x3f;
	pk2.bits.pa = parity[pk2.c & 0x3f];
	pk3.bits.pa = parity[pk3.c & 0x3f] ^ 1;
	pk4.bits.pa = parity[pk4.c & 0x3f];
	cbmp += sprintf (cbmp, "%c%c%c%c", pk1.c, pk2.c, pk3.c, pk4.c);
	csize++;
	pktcnt++;

	if (csize == MAX_PACKET_COUNT)
		out_packet (2, 1, 0, 0);
}

static int compress_bitmap (FILE *bitmapf)
{
	int band;
	unsigned char c1, c2, c3;
	int cnt;			/* count of characters processed in a band */
	int pcnt;			/* count of chars for each packet */

	if (fgets (cbm, 200, bitmapf) <= 0)
		return 0;

	if (strncmp (cbm, "P4", 2))
	{
		message ("Wrong file format.\n");
		message ("file position: %lx\n", ftell (bitmapf));
		errorexit();
	}
	/* bypass the comment line */
	do
	{
		fgets (cbm, 200, bitmapf);
	} while (cbm[0] == '#');
	/* read the bitmap's dimensions */
	if (sscanf (cbm, "%d %d", &bmwidth, &bmheight) < 2)
	{
		message ("Bitmap file with wrong size fields.\n");
		errorexit();
	}
	bmwidth = (bmwidth + 7) / 8;
	/* adjust top and left margins */
	if (topskip) /* we can't do seek from a pipe */
		bitmap_seek (bitmapf, bmwidth * topskip);

	bmcnt = 0; /* Needed, otherwise corrupt all but first page */

	/* now we process the real data */
	for (band = 0; linecnt < lines_by_page; band++)
	{
		pktcnt = 0;
		/* setup packet size */
		if (((lines_by_page - linecnt) > ROWS_BY_BAND))
			cnt = LINE_SIZE * ROWS_BY_BAND;
		else
			cnt = LINE_SIZE * (lines_by_page - linecnt);

		message ("cnt: %d, band: %d, linecnt: %d\n", cnt, band, linecnt);
		c1 = get_bitmap (bitmapf);
		c2 = get_bitmap (bitmapf);
		cnt -= 2;
		pcnt = 1;

		while (cnt)
		{
			if ((c1 == c2) && (cnt > 2))
			{
				pcnt++;
				c2 = get_bitmap (bitmapf);
				cnt--;
				continue;
			}
			if (cnt==2)
			{
				/* leave at least 2 bytes */
				while (pcnt > 258)
				{
					out_packet (1, 255, c1, c1);
					pcnt -= 257;
				}
				/* one more if too large for one packet */
				if (pcnt > 256)
				{
					out_packet (1, 253, c1, c1);
					pcnt -= 255;
				}
				out_packet (1, (pcnt - 1), c1, c2);
				break;
			}
			if ((cnt == 3) || (cnt == 4))
			{
				if (pcnt > 1)
				{
					while (pcnt > 259)
					{
						out_packet (1, 255, c1, c1);
						pcnt -= 257;
					}
					if (pcnt > 257)
					{
						out_packet (1, 253, c1, c1);
						pcnt -= 255;
					}
					out_packet (1, (pcnt - 2), c1, c1);
					c3 = get_bitmap (bitmapf);
					if (cnt == 3)
					{
						out_packet (1, 0, c2, c3);
					} else {
						c1 = get_bitmap (bitmapf);
						out_packet (0, c2, c3, c1);
					}
				} else {
					c3 = get_bitmap (bitmapf);
					if (cnt == 3)
					{
						out_packet (0, c1, c2, c3);
					} else {
						out_packet (1, 0, c1, c2);
						c1 = get_bitmap (bitmapf);
						out_packet (1, 0, c3, c1);
					}
				}
				break;
			}
			if (pcnt>1)
			{
				while (pcnt > 258)
				{
					out_packet (1, 255, c1, c1);
					pcnt -= 257;
				}
				if (pcnt > 256)
				{
					out_packet (1, 253, c1, c1);
					pcnt -= 255;
				}
				out_packet (1, (pcnt - 1), c1, c2);
				pcnt = 1;
				c1 = get_bitmap (bitmapf);
				c2 = get_bitmap (bitmapf);
				cnt -= 2;
			} else {
				c3 = get_bitmap (bitmapf);
				out_packet (0, c1, c2, c3);
				c1 = get_bitmap (bitmapf);
				c2 = get_bitmap (bitmapf);
				cnt -= 3;
			}
		}
		out_packet (2, 0, 0, 0);
	}
	fflush (cbmf);
	fseek (cbmf, 0, SEEK_SET);
	return 1;
}

/* End of Rildo Pragana constants and functions */

void INLINE errorexit (void)
{
#ifdef DEBUG
	int *i = 0;
	(*i)++;
#endif
	if (cbmf)
		fclose (cbmf);
	exit (1);
}

void INLINE dataout (int data)
{
	outb (data, DATA);
}

void INLINE ctrlout (int cmd)
{
	outb (cmd, CONTROL);
}

int INLINE ctrlin (void)
{
	return inb (CONTROL);
}

void INLINE checkctrl (int control)
{
	int ctrl = inb (CONTROL);
	if ((ctrl & 0x1f) != (control & 0x1f))
	{
		message ("Error, wrong control : %x instead of %x\n", ctrl, control);
		errorexit();
	}
}

int INLINE statusin (void)
{
	return inb (STATUS);
}

void INLINE checkstatus (int status)
{
	int stat = statusin();
	if ((stat & 0xf8) != (status & 0xf8))
	{
		message ("Error, wrong status : %x instead of %x\n", stat, status);
		errorexit();
	}
}

int INLINE cmdout (int cmd)
{
	int stat;
	ctrlout (cmd);
	usleep (1);
	stat = statusin();
	checkctrl (cmd);
	return stat;
}

void INLINE checkcmdout (int cmd, int status, int mask)
{
	int stat = cmdout (cmd);
	if ((stat & mask) != (status & mask))
	{
		message ("Error, wrong status (checkcmdout) :"
			 " %x instead of %x (mask : %x)\n", stat, status, mask);
		errorexit();
	}
}

int INLINE cmddataouts (int cmd, int data, int sleep)
{
	int stat;
	ctrlout (cmd);
	usleep (sleep);
	stat = statusin();
	dataout (data);
	checkctrl (cmd);
	return stat;
}

void INLINE cmddataout (int cmd, int data)
{
	cmddataouts (cmd, data, 10);
}

void INLINE checkcmddataouts (int cmd, int data, int status, int mask, int sleep)
{
	int stat = cmddataouts (cmd, data, sleep);
	if ((stat & mask) != (status & mask))
	{
		message ("Error, wrong status (checkcmddataout) :"
			 " %x instead of %x (mask : %x)\n", stat, status, mask);
		errorexit();
	}
}

void INLINE checkcmddataout (int cmd, int data, int status, int mask)
{
	checkcmddataouts (cmd, data, status, mask, 15);
}

void INLINE data6out (int data)
{
	// Must be : cmdout (2, 4[e6])
	checkcmddataout (0x06, data, 0x70, 0x70);
	ctrlout (0x06);
	usleep (10);
	checkcmdout (0x7, 0x70, 0x70);
	checkcmdout (0x6, 0x70, 0x70);
	ctrlout (0x06);
}

void INLINE data64out (int *data, int start, int end)
{
	int i;
	// Must be : cmdout (2, 4[e6])
	checkcmddataout (0x06, data[start], 0x70, 0x70);
	for (i = start + 1; i < end; i += 2)
	{
		ctrlout (0x06);
		checkcmdout (0x07, 0x70, 0x70);
		checkcmddataout (0x06, data[i], 0x70, 0x70);

		ctrlout (0x04);
		checkcmdout (0x05, 0x70, 0x70);
		checkcmddataout (0x04, data[i + 1], 0x70, 0x70);
	}
	ctrlout (0x06);
	checkcmdout (0x7, 0x70, 0x70);
	checkcmdout (0x6, 0x70, 0x70);
	ctrlout (0x06);
}

/* band : index of the band
 * size : size of the band
 * type : 0 : classic
 *        1 : truncated (never used)
 * white : should we send only a white band ?
 * timeout : should we timeout (1), or wait for paper forever (0)
 */
static int print_band (int band, int size, int type, int white, int timeout)
{
	int i;
	unsigned char *buf;
	int ret;

	buf = cbm;

	message ("Initing band(%d - %d - %d - %d - %d)...\n",
		 band, size, type, white, timeout);

	if (type == 1)
	{ // Quick init (band truncated), never used
		checkcmddataouts (0x04, 0xff, 0x70, 0x70, 1);
	} else { //Normal init
		ctrlout (0x02);
		checkctrl (0xc2);
		checkcmddataouts (0x06, 0x80, 0x70, 0x70, 1);
		checkcmdout (0x07, 0x70, 0x70);
		checkcmddataouts (0x06, 0xff, 0x70, 0x70, 1);
	}
	ctrlout (0x04);
	ctrlout (0x05);

	message ("Waiting for ready status...\n");
	statusin();
	if (((ret = statusin()) & 0xf0) != 0x70)
	{
		struct timeval ltv; /* Begin time */
		struct timeval itv; /* Last init time */
		struct timeval ntv; /* Current time */
		gettimeofday (&ltv, NULL);
		gettimeofday (&itv, NULL);
		do
		{
			message ("%x ", ret);
			usleep (1);
			gettimeofday (&ntv, NULL);
			if (((ntv.tv_usec - itv.tv_usec)
			     + ((ntv.tv_sec - itv.tv_sec) * 1000000)) > 1000000)
			{ // Reinit every second
				message ("Reiniting band...\n");
				statusin();
				if (type == 1)
				{ // Quick init (band truncated), never used
					checkcmddataouts (0x04, 0xff, 0x70, 0x70, 1);
				} else { //Normal init
					ctrlout (0x02);
					checkctrl (0xc2);
					checkcmddataouts (0x06, 0x80, 0x70, 0x70, 1);
					//      ctrlout (0x06);
					checkcmdout (0x07, 0x70, 0x70);
					checkcmddataouts (0x06, 0xff, 0x70, 0x70, 1);
				}
				ctrlout (0x04);
				ctrlout (0x05);
				gettimeofday (&itv, NULL);
			}
			if (((ntv.tv_usec - ltv.tv_usec)
			     + ((ntv.tv_sec - ltv.tv_sec) * 1000000)) > 15000000)
			{ // 15 seconds timeout
				if (timeout)
				{
					message ("Band initialisation failed (0x%x)\n",
						 statusin());
					return 0;
				} else {
					message ("Waiting for paper... (0x%x)\n",
						 statusin());
					while (((ret = statusin()) & 0xf0) == 0xF0)
					{
						gettimeofday (&ntv, NULL);
						if (((ntv.tv_usec - ltv.tv_usec)
						     + ((ntv.tv_sec - ltv.tv_sec) * 1000000))
						    > 1800000000)
						{ //30 minutes timeout
						message ("Timed out waiting for paper. (0x%x)\n",
							 statusin());
						return 0;
						}
						usleep (1);
					}
					timeout = 1;
					gettimeofday (&ltv, NULL);
				}
			}
		} while (((ret = statusin()) & 0xf0) != 0x70);
		message ("Band inited (0x%x, %lu)\n", statusin(),
			 ((ntv.tv_usec - ltv.tv_usec) + ((ntv.tv_sec - ltv.tv_sec) * 1000000)));
	} else {
		message ("Band inited (0x%x, 0)\n", statusin());
	}

	/* data */

	if (white)
	{
		for (i = 0; i < 242; i++)
		{
			dataout (0x7f);
			dataout (0x83);
			dataout (0x40);
			dataout (0x80);
		}
		dataout (0x4d);
		dataout (0x83);
		dataout (0x40);
	} else {
		for (i = 0; i < size; i++)
		{
			dataout (*buf++);
			dataout (*buf++);
			dataout (*buf++);
			dataout (*buf++);
		}
	}

	checkctrl (0xc5);
	checkcmddataouts (0x04, 0x89, 0x70, 0x70, 3000);
	ctrlout (0x06);
	checkcmdout (0x07, 0x70, 0x70);
	checkcmdout (0x06, 0x70, 0x70);
	ctrlout (0x06);

	return ret;
}

static void reset_printer (struct printer *prt)
{
	int i = 0;
	int sig = 0;
	int ret = 0;
	int offset = 0;

	message ("Resetting %s...", prt->name);
	
	dataout (0x24);
	dataout (0x06);
	usleep (100);
	
	ctrlout (0x0a);
	ctrlout (0x0a);
	ctrlout (0x0e);
	usleep (1000000); //16
	
	dataout (0x24);
	checkctrl (0xce);
	ctrlout (0x06);
	usleep (150); /* 100-250 */

	{
		int stat = statusin();

		switch (stat /*& 0xf8*/)
		{
			case 0x3e:
				printf ("ok\n");
				break;
			case 0x5e:
				printf ("failed, check cables\n");
				errorexit();
			default:
				printf ("failed, error code 0x%x\n", stat);
				errorexit();
		}
	}

	checkctrl (0xc6);
	ctrlout (0x07);
	ctrlout (0x07);
	ctrlout (0x04);
	usleep (40);
	
	checkstatus (0xde);
	checkctrl (0xc4);
	ctrlout (0x06);
	usleep (40);
	
	checkstatus (0xfe);
	usleep (10);
	
	checkctrl (0xc6);
	ctrlout (0x06);
	sig = 0; /* true if a 5e has been received */
	i = 0; /* 0e - 4e count */
	while (1)
	{
		ret = cmdout(0x02) & 0xf8;
		if (ret == 0x08)
		{
			i++;
		} else if (ret == 0x18) {
			i = 0;
		} else {
			message ("Error, wrong status (init 2nd loop) :"
				 " %x instead of 0x[01]8\n", ret);
			errorexit();
		}

		if (sig && (i == 21))
			break;

		ret = cmdout(0x00) & 0xf8;
		if (ret == 0x48)
		{
			i++;
		} else if (ret == 0x58) {
			i = 0;
			sig = 1;
		} else {
			message ("Error, wrong status (init 2nd loop) :"
				 " %x instead of 0x[45]8\n", ret);
			errorexit();
		}
	}

	checkcmdout (0x06, 0x78, 0x78);
	ctrlout (0x04);
	checkcmdout (0x0c, 0x28, 0x78);
	ctrlout (0x0c);
	usleep (15);
	
	dataout (0x20);
	checkctrl (0xcc);
	checkcmdout (0x06, 0x38, 0x78);
	ctrlout (0x07);
	ctrlout (0x07);
	ctrlout (0x04);
	usleep (40);
	
	checkstatus (0xde);
	checkctrl (0xc4);
	ctrlout (0x06);
	usleep (40);
	
	checkstatus (0xfe);
	sleep (2);

	for (i = 0; i < 12287; i++)
		dataout (0);

	usleep (500);
	
	checkstatus (0xfe);
	dataout (0xa0);
	checkctrl (0xc6);
	ctrlout (0x06);
	checkcmdout (0x07, 0x78, 0x78);
	ctrlout (0x06);
	usleep (10);
	
	checkstatus (0xfe);
	dataout (0x00);
	checkctrl (0xc6);
	ctrlout (0x04);
	checkcmdout (0x05, 0x78, 0x78);
	ctrlout (0x04);
	usleep (20);
	
	checkstatus (0xfe);
	dataout (0xa0);
	checkctrl (0xc4);
	ctrlout (0x06);
	checkcmdout (0x07, 0x78, 0x78);
	checkcmdout (0x06, 0x78, 0x78);
	ctrlout (0x06);
	offset = 0;

	message ("Printer reseted.\n");
}

static int print_page (struct printer *prt, int page)
{
	int i = 0;
	int inited = 0; // 0: the printer is not ready,
			// 1: started to init the page,
			// 2: started to print (there is paper)
	int ret = 0;
	int offset = 0;
	int len = 0;
	int size;

	int *data = pagedata;

	struct timeval printinittv;
	struct timeval printnewtv;

	message ("Sending page...\n");
	i = 0; //Band counter

	gettimeofday (&printinittv, NULL);

	while (1)
	{
		ret = cmdout (0);
		if (!inited)
		{
			gettimeofday (&printnewtv, NULL);
			if ((printnewtv.tv_sec - printinittv.tv_sec) > 3)
			{
				reset_printer (prt);
				gettimeofday (&printinittv, NULL);
			}
		}

		if ((cmdout(2) & 0xf0) == 0x40)
		{ //0x40 or 0x48
			if (!inited)
				inited = 1;

			len = -data[offset];
			if (len == 260)
			{
				offset = 0;
				data = bandinit;
			} else if (len > 255) {
				message ("Sending band %d...\n", i);
				if ((! feof (cbmf)) && fread (&size, 1, sizeof (int), cbmf))
				{
					int type = len - 256;

					size = size & 0x0FFF;

					fread (cbm, size * 4, 1, cbmf);
					ret = print_band (i, size, type, 0,
							  (inited - 1) || (i == 0));
					if (!ret)
						return 0;
					else if ((ret & 0xf0) != 0x70)
						inited = 2;
				} else {
					break;
				}
				offset++;

				i++;
			} else if (len > 1) {
				if (data[offset + 2] == -1)
				{
					data[offset + 2] = (i % 2) + 1;
					data64out (data, offset + 1, offset + 1 + len);
					data[offset + 2] = -1;
				} else {
					data64out (data, offset + 1, offset + 1 + len);
				}
				offset += len + 1;
			} else {
				data6out (data[offset + 1]);
				offset += 2;
			}
		}
	}
	message ("OK\n");
	return 1;
}

static struct printer *get_printer (const char *name)
{
	int i;
	for (i = 0; printers[i].name; i++)
		if (strcmp (printers[i].name, name) == 0)
			return &printers[i];
	return NULL;
}

int main (int argc, char **argv)
{
	int c;
	int simulate = 0;
	int reset_only = 0;
	int reset = 0;
	int tfd;
	int lbp460 = 0;

	struct printer *prt = get_printer ("LBP-660");

	FILE *bitmapf = stdin;

	while ((c = getopt (argc, argv, "Rrt:l:sf:c")) != -1)
	{
		switch (c)
		{
		case 'R':
			reset_only = 1;
			reset = 1;
			break;
		case 'r':
			reset = 1;
			break;
		case 'c':
			prt = get_printer ("LBP-460");
			lbp460 = 1;
			break;
		case 't':
			sscanf (optarg, "%d", &topskip);
			break;
		case 'l':
			sscanf (optarg, "%d", &leftskip);
			break;
		case 's':
			simulate++;
			break;
		case 'f':
			bitmapf = fopen (optarg, "r");
			if (!bitmapf)
			{
				message ("File not found or unreadable\n");
				errorexit();
			}
		}
	}

	if (ioperm (DATA, 3, 1))
	{
		message ("Sorry, you were not able to gain access to the ports\n");
		message ("You must be root to run this program\n");
		errorexit();
	}

	/* select the right page resolution */
	lines_by_page = prt->lines_by_page;
	
	message ("%s\n", lbp460 ?
		 "Running with LBP-460 page resolution (600x300)." :
		 "Running with LBP-660 page resolution (600x600).");

	if ((reset && !simulate) || (lbp460))
		reset_printer (prt);

	if (!reset_only)
	{
		/* pages printing loop */
		struct timeval ltv;
		struct timeval ntv;

		int page;
		
		for (page = 0;;page++)
		{
			/* temporary file to store our results */
			char tmpname[] = "/tmp/lbp660-XXXXXX";
			if ((tfd = mkstemp (tmpname)) < 0)
			{
				message ("Can't open a temporary file.\n");
				errorexit();
			}
			cbmf = fdopen (tfd, "w+");
			unlink (tmpname);

			if (! compress_bitmap (bitmapf))
				break;

			/* If simulating, skip actual printing. */
			if (simulate)
				goto page_printed;
			
			if (page != 0)
			{
				gettimeofday (&ntv, NULL);
				/* delay between pages */
				usleep (PAGE_DELAY - ((ntv.tv_usec - ltv.tv_usec)
						      + ((ntv.tv_sec - ltv.tv_sec)
							 * 1000000)));
			}
			
			if (! print_page (prt, page))
			{
				message ("Error, cannot print this page.\n");
				reset_printer (prt);
				errorexit();
			}
			gettimeofday (&ltv, NULL);

		page_printed:
			fclose (cbmf);
			next_page (bitmapf, page);
		}
	}

	if (bitmapf != stdin)
		fclose (bitmapf);

	return 0;
}
