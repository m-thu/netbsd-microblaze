/*	$NetBSD: boot.c,v 1.20 2014/03/26 17:43:11 christos Exp $	*/

/*-
 * Copyright (C) 1999 Tsubai Masanari.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>
#include <lib/libsa/loadfile.h>

#include <machine/apcall.h>
#include <machine/bootinfo.h>
#include <machine/cpu.h>
#include <machine/romcall.h>

void boot(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

void mips1_flushicache(void *, int);
extern char _edata[], _end[];

/* version strings in vers.c (generated by newvers.sh) */
extern const char bootprog_name[];
extern const char bootprog_rev[];

struct apbus_sysinfo *_sip;
int apbus;

char *devs[] = { "sd", "fh", "fd", NULL, NULL, "rd", "st" };
char *kernels[] = { "/netbsd", "/netbsd.gz", NULL };

#ifdef BOOT_DEBUG
# define DPRINTF printf
#else
# define DPRINTF while (0) printf
#endif

void
boot(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
    uint32_t a5)
{
	int fd, i;
	char *netbsd = "";
	int maxmem;
	u_long marks[MARK_MAX];
	char devname[32], file[32];
	void (*entry)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
	    uint32_t);
	struct btinfo_symtab bi_sym;
	struct btinfo_bootarg bi_arg;
	struct btinfo_bootpath bi_bpath;
	struct btinfo_systype bi_sys;
	int loadflag;

	/* Clear BSS. */
	memset(_edata, 0, _end - _edata);

	/*
	 * XXX a3 contains:
	 *     maxmem (nws-3xxx)
	 *     argv   (apbus-based machine)
	 */
	if (a3 >= 0x80000000)
		apbus = 1;
	else
		apbus = 0;

	if (apbus)
		_sip = (void *)a4;

	printf("%s Secondary Boot, Revision %s\n",
	    bootprog_name, bootprog_rev);

	if (apbus) {
		char *bootdev = (char *)a1;
		int argc = a2;
		char **argv = (char **)a3;

		DPRINTF("APbus-based system\n");

		DPRINTF("argc = %d\n", argc);
		for (i = 0; i < argc; i++) {
			DPRINTF("argv[%d] = %s\n", i, argv[i]);
			if (argv[i][0] != '-' && *netbsd == 0)
				netbsd = argv[i];
		}
		maxmem = _sip->apbsi_memsize;
		maxmem -= 0x100000;	/* reserve 1MB for ROM monitor */

		DPRINTF("howto = 0x%x\n", a0);
		DPRINTF("bootdev = %s\n", (char *)a1);
		DPRINTF("bootname = %s\n", netbsd);
		DPRINTF("maxmem = 0x%x\n", maxmem);

		/* XXX use "sonic()" instead of "tftp()" */
		if (strncmp(bootdev, "tftp", 4) == 0)
			bootdev = "sonic";

		strcpy(devname, bootdev);
		if (strchr(devname, '(') == NULL)
			strcat(devname, "()");
	} else {
		int bootdev = a1;
		char *bootname = (char *)a2;
		int ctlr, unit, part, type;

		DPRINTF("HB system.\n");

		/* bootname is "/boot" by default on HB system. */
		if (bootname && strcmp(bootname, "/boot") != 0)
			netbsd = bootname;
		maxmem = a3;

		DPRINTF("howto = 0x%x\n", a0);
		DPRINTF("bootdev = 0x%x\n", a1);
		DPRINTF("bootname = %s\n", netbsd);
		DPRINTF("maxmem = 0x%x\n", maxmem);

		ctlr = BOOTDEV_CTLR(bootdev);
		unit = BOOTDEV_UNIT(bootdev);
		part = BOOTDEV_PART(bootdev);
		type = BOOTDEV_TYPE(bootdev);

		if (devs[type] == NULL) {
			printf("unknown bootdev (0x%x)\n", bootdev);
			_rtt();
		}

		snprintf(devname, sizeof(devname), "%s(%d,%d,%d)",
		    devs[type], ctlr, unit, part);
	}

	printf("Booting %s%s\n", devname, netbsd);

	/* use user specified kernel name if exists */
	if (*netbsd) {
		kernels[0] = netbsd;
		kernels[1] = NULL;
	}

	loadflag = LOAD_KERNEL;
	if (devname[0] == 'f')	/* XXX */
		loadflag &= ~LOAD_BACKWARDS;

	marks[MARK_START] = 0;

	for (i = 0; kernels[i]; i++) {
		snprintf(file, sizeof(file), "%s%s", devname, kernels[i]);
		DPRINTF("trying %s...\n", file);
		fd = loadfile(file, marks, loadflag);
		if (fd != -1)
			break;
	}
	if (kernels[i] == NULL)
		_rtt();

	DPRINTF("entry = 0x%x\n", (int)marks[MARK_ENTRY]);
	DPRINTF("ssym = 0x%x\n", (int)marks[MARK_SYM]);
	DPRINTF("esym = 0x%x\n", (int)marks[MARK_END]);

	bi_init(BOOTINFO_ADDR);

	bi_sym.nsym = marks[MARK_NSYM];
	bi_sym.ssym = marks[MARK_SYM];
	bi_sym.esym = marks[MARK_END];
	bi_add(&bi_sym, BTINFO_SYMTAB, sizeof(bi_sym));

	bi_arg.howto = a0;
	bi_arg.bootdev = a1;
	bi_arg.maxmem = maxmem;
	bi_arg.sip = (int)_sip;
	bi_add(&bi_arg, BTINFO_BOOTARG, sizeof(bi_arg));

	strcpy(bi_bpath.bootpath, file);
	bi_add(&bi_bpath, BTINFO_BOOTPATH, sizeof(bi_bpath));

	bi_sys.type = apbus ? NEWS5000 : NEWS3400;		/* XXX */
	bi_add(&bi_sys, BTINFO_SYSTYPE, sizeof(bi_sys));

	entry = (void *)marks[MARK_ENTRY];

	if (apbus)
		apcall_flushcache();
	else
		mips1_flushicache(entry, marks[MARK_SYM] - marks[MARK_ENTRY]);

	printf("\n");
	(*entry)(a0, a1, a2, a3, a4, a5);
}

void
putchar(int x)
{
	char c = x;

	if (apbus)
		apcall_write(1, &c, 1);
	else
		rom_write(1, &c, 1);
}

int
getchar(void)
{
	unsigned char c = '\0';
	int i;

	for (;;) {
		i = apbus ? apcall_read(1, &c, 1) : rom_read(1, &c, 1);
		if (i == 1)
			break;
		if (i != -2 && i != 0)
			return -1;
	}
	return c;
}

void
_rtt(void)
{
	if (apbus)
		apcall_exit(8);
	else
		rom_halt();

	for (;;);
}