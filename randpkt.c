/*
 * randpkt.c
 * ---------
 * Creates random packet traces. Useful for debugging sniffers by testing
 * assumptions about the veracity of the data found in the packet.
 *
 * $Id$
 *
 * Copyright (C) 1999 by Gilbert Ramirez <gram@alumni.rice.edu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef NEED_GETOPT_H
#include "getopt.h"
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <time.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "wiretap/wtap.h"

#define array_length(x)	(sizeof x / sizeof x[0])

/* Types of produceable packets */
enum {
	PKT_ARP,
	PKT_BGP,
	PKT_BVLC,
	PKT_DNS,
	PKT_ETHERNET,
	PKT_FDDI,
	PKT_GIOP,
	PKT_ICMP,
	PKT_IP,
	PKT_LLC,
	PKT_M2M,
	PKT_MEGACO,
	PKT_NBNS,
	PKT_NCP2222,
	PKT_SCTP,
	PKT_SYSLOG,
	PKT_TCP,
	PKT_TDS,
	PKT_TR,
	PKT_UDP
};

typedef struct {
	const char	*abbrev;
	const char	*longname;
	int		produceable_type;
	guint8		*sample_buffer;
	int		sample_wtap_encap;
	int		sample_length;
} pkt_example;

/* Ethernet, indicating ARP */
guint8 pkt_arp[] = {
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0x00, 0x00,
	0x32, 0x25, 0x0f, 0xff,
	0x08, 0x06
};

/* Ethernet+IP+UDP, indicating DNS */
guint8 pkt_dns[] = {
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0x3c,
	0xc5, 0x9e, 0x40, 0x00,
	0xff, 0x11, 0xd7, 0xe0,
	0xd0, 0x15, 0x02, 0xb8,
	0x0a, 0x01, 0x01, 0x63,

	0x05, 0xe8, 0x00, 0x35,
	0xff, 0xff, 0x2a, 0xb9,
	0x30
};

/* Ethernet+IP, indicating ICMP */
guint8 pkt_icmp[] = {
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0x54,
	0x8f, 0xb3, 0x40, 0x00,
	0xfd, 0x01, 0x8a, 0x99,
	0xcc, 0xfc, 0x66, 0x0b,
	0xce, 0x41, 0x62, 0x12
};

/* Ethernet, indicating IP */
guint8 pkt_ip[] = {
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01,
	0x08, 0x00
};

/* TR, indicating LLC */
guint8 pkt_llc[] = {
	0x10, 0x40, 0x68, 0x00,
	0x19, 0x69, 0x95, 0x8b,
	0x00, 0x01, 0xfa, 0x68,
	0xc4, 0x67
};

/* Ethernet, indicating WiMAX M2M */
guint8 pkt_m2m[] = {
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0x00, 0x00,
	0x32, 0x25, 0x0f, 0xff,
	0x08, 0xf0
};

/* Ethernet+IP+UDP, indicating NBNS */
guint8 pkt_nbns[] = {
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0x3c,
	0xc5, 0x9e, 0x40, 0x00,
	0xff, 0x11, 0xd7, 0xe0,
	0xd0, 0x15, 0x02, 0xb8,
	0x0a, 0x01, 0x01, 0x63,

	0x00, 0x89, 0x00, 0x89,
	0x00, 0x00, 0x2a, 0xb9,
	0x30
};

/* Ethernet+IP+UDP, indicating syslog */
guint8 pkt_syslog[] = {
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0x64,
	0x20, 0x48, 0x00, 0x00,
	0xfc, 0x11, 0xf8, 0x03,
	0xd0, 0x15, 0x02, 0xb8,
	0x0a, 0x01, 0x01, 0x63,

	0x05, 0xe8, 0x02, 0x02,
	0x00, 0x50, 0x51, 0xe1,
	0x3c
};

/* TR+LLC+IP, indicating TCP */
guint8 pkt_tcp[] = {
	0x10, 0x40, 0x68, 0x00,
	0x19, 0x69, 0x95, 0x8b,
	0x00, 0x01, 0xfa, 0x68,
	0xc4, 0x67,

	0xaa, 0xaa, 0x03, 0x00,
	0x00, 0x00, 0x08, 0x00,

	0x45, 0x00, 0x00, 0x28,
	0x0b, 0x0b, 0x40, 0x00,
	0x20, 0x06, 0x85, 0x37,
	0xc0, 0xa8, 0x27, 0x01,
	0xc0, 0xa8, 0x22, 0x3c
};

/* Ethernet+IP, indicating UDP */
guint8 pkt_udp[] = {
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0x3c,
	0xc5, 0x9e, 0x40, 0x00,
	0xff, 0x11, 0xd7, 0xe0,
	0xd0, 0x15, 0x02, 0xb8,
	0x0a, 0x01, 0x01, 0x63
};

/* Ethernet+IP+UDP, indicating BVLC */
guint8 pkt_bvlc[] = {
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0x3c,
	0xc5, 0x9e, 0x40, 0x00,
	0xff, 0x11, 0x01, 0xaa,
	0xc1, 0xff, 0x19, 0x1e,
	0xc1, 0xff, 0x19, 0xff,
	0xba, 0xc0, 0xba, 0xc0,
	0x00, 0xff, 0x2d, 0x5e,
	0x81
};

/* TR+LLC+IPX, indicating NCP, with NCP Type == 0x2222 */
guint8 pkt_ncp2222[] = {
	0x10, 0x40, 0x00, 0x00,
	0xf6, 0x7c, 0x9b, 0x70,
	0x68, 0x00, 0x19, 0x69,
	0x95, 0x8b, 0xe0, 0xe0,
	0x03, 0xff, 0xff, 0x00,
	0x25, 0x02, 0x11, 0x00,
	0x00, 0x74, 0x14, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x01, 0x04, 0x51, 0x00,
	0x00, 0x00, 0x04, 0x00,
	0x02, 0x16, 0x19, 0x7a,
	0x84, 0x40, 0x01, 0x22,
	0x22
};

/* Ethernet+IP+TCP, indicating GIOP */
guint8 pkt_giop[] = {
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0xa6,
	0x00, 0x2f, 0x40, 0x00,
	0x40, 0x06, 0x3c, 0x21,
	0x7f, 0x00, 0x00, 0x01,
	0x7f, 0x00, 0x00, 0x01,

	0x30, 0x39, 0x04, 0x05,
	0xac, 0x02, 0x1e, 0x69,
	0xab, 0x74, 0xab, 0x64,
	0x80, 0x18, 0x79, 0x60,
	0xc4, 0xb8, 0x00, 0x00,
	0x01, 0x01, 0x08, 0x0a,
	0x00, 0x00, 0x48, 0xf5,
	0x00, 0x00, 0x48, 0xf5,

	0x47, 0x49, 0x4f, 0x50,
	0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x30,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x01,
	0x01
};

/* Ethernet+IP+TCP, indicating BGP */
guint8 pkt_bgp[] = {
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0xa6,
	0x00, 0x2f, 0x40, 0x00,
	0x40, 0x06, 0x3c, 0x21,
	0x7f, 0x00, 0x00, 0x01,
	0x7f, 0x00, 0x00, 0x01,

	0x30, 0x39, 0x00, 0xb3,
	0xac, 0x02, 0x1e, 0x69,
	0xab, 0x74, 0xab, 0x64,
	0x80, 0x18, 0x79, 0x60,
	0xc4, 0xb8, 0x00, 0x00,
	0x01, 0x01, 0x08, 0x0a,
	0x00, 0x00, 0x48, 0xf5,
	0x00, 0x00, 0x48, 0xf5,

	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
};

/* Ethernet+IP+TCP, indicating TDS NetLib */
guint8 pkt_tds[] = {
	0x00, 0x50, 0x8b, 0x0d,
	0x7a, 0xed, 0x00, 0x08,
	0xa3, 0x98, 0x39, 0x81,
	0x08, 0x00,

	0x45, 0x00, 0x03, 0x8d,
	0x90, 0xd4, 0x40, 0x00,
	0x7c, 0x06, 0xc3, 0x1b,
	0xac, 0x14, 0x02, 0x22,
	0x0a, 0xc2, 0xee, 0x82,

	0x05, 0x99, 0x08, 0xf8,
	0xff, 0x4e, 0x85, 0x46,
	0xa2, 0xb4, 0x42, 0xaa,
	0x50, 0x18, 0x3c, 0x28,
	0x0f, 0xda, 0x00, 0x00,
};

/* Ethernet+IP, indicating SCTP */
guint8 pkt_sctp[] = {
	0x00, 0xa0, 0x80, 0x00,
	0x5e, 0x46, 0x08, 0x00,
	0x03, 0x4a, 0x00, 0x35,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0x7c,
	0x14, 0x1c, 0x00, 0x00,
	0x3b, 0x84, 0x4a, 0x54,
	0x0a, 0x1c, 0x06, 0x2b,
	0x0a, 0x1c, 0x06, 0x2c,
};


/* Ethernet+IP+SCTP, indicating MEGACO */
guint8 pkt_megaco[] = {
	0x00, 0xa0, 0x80, 0x00,
	0x5e, 0x46, 0x08, 0x00,
	0x03, 0x4a, 0x00, 0x35,
	0x08, 0x00,

	0x45, 0x00, 0x00, 0x7c,
	0x14, 0x1c, 0x00, 0x00,
	0x3b, 0x84, 0x4a, 0x54,
	0x0a, 0x1c, 0x06, 0x2b,
	0x0a, 0x1c, 0x06, 0x2c,

	0x40, 0x00, 0x0b, 0x80,
	0x00, 0x01, 0x6f, 0x0a,
	0x6d, 0xb0, 0x18, 0x82,
	0x00, 0x03, 0x00, 0x5b,
	0x28, 0x02, 0x43, 0x45,
	0x00, 0x00, 0xa0, 0xbd,
	0x00, 0x00, 0x00, 0x07,
};

/* This little data table drives the whole program */
pkt_example examples[] = {
	{ "arp", "Address Resolution Protocol",
		PKT_ARP,	pkt_arp,	WTAP_ENCAP_ETHERNET,	array_length(pkt_arp) },

	{ "bgp", "Border Gateway Protocol",
		PKT_BGP,	pkt_bgp,	WTAP_ENCAP_ETHERNET,	array_length(pkt_bgp) },

	{ "bvlc", "BACnet Virtual Link Control",
		PKT_BVLC,	pkt_bvlc,	WTAP_ENCAP_ETHERNET,	array_length(pkt_bvlc) },

	{ "dns", "Domain Name Service",
		PKT_DNS,	pkt_dns,	WTAP_ENCAP_ETHERNET,	array_length(pkt_dns) },

	{ "eth", "Ethernet",
		PKT_ETHERNET,	NULL,		WTAP_ENCAP_ETHERNET,	0 },

	{ "fddi", "Fiber Distributed Data Interface",
		PKT_FDDI,	NULL,		WTAP_ENCAP_FDDI,	0 },

	{ "giop", "General Inter-ORB Protocol",
		PKT_GIOP,	pkt_giop,	WTAP_ENCAP_ETHERNET,	array_length(pkt_giop) },

	{ "icmp", "Internet Control Message Protocol",
		PKT_ICMP,	pkt_icmp,	WTAP_ENCAP_ETHERNET,	array_length(pkt_icmp) },

	{ "ip", "Internet Protocol",
		PKT_IP,		pkt_ip,		WTAP_ENCAP_ETHERNET,	array_length(pkt_ip) },

	{ "llc", "Logical Link Control",
		PKT_LLC,	pkt_llc,	WTAP_ENCAP_TOKEN_RING,	array_length(pkt_llc) },

	{ "m2m", "WiMAX M2M Encapsulation Protocol",
		PKT_M2M,	pkt_m2m,	WTAP_ENCAP_ETHERNET,	array_length(pkt_m2m) },

	{ "megaco", "MEGACO",
		PKT_MEGACO,	pkt_megaco,	WTAP_ENCAP_ETHERNET,	array_length(pkt_megaco) },

	{ "nbns", "NetBIOS-over-TCP Name Service",
		PKT_NBNS,	pkt_nbns,	WTAP_ENCAP_ETHERNET,	array_length(pkt_nbns) },

	{ "ncp2222", "NetWare Core Protocol",
		PKT_NCP2222,	pkt_ncp2222,	WTAP_ENCAP_TOKEN_RING,	array_length(pkt_ncp2222) },

	{ "sctp", "Stream Control Transmission Protocol",
		PKT_SCTP,	pkt_sctp,	WTAP_ENCAP_ETHERNET,	array_length(pkt_sctp) },

	{ "syslog", "Syslog message",
		PKT_SYSLOG,	pkt_syslog,	WTAP_ENCAP_ETHERNET,	array_length(pkt_syslog) },

	{ "tds", "TDS NetLib",
		PKT_TDS,	pkt_tds,	WTAP_ENCAP_ETHERNET,	array_length(pkt_tds) },

	{ "tcp", "Transmission Control Protocol",
		PKT_TCP,	pkt_tcp,	WTAP_ENCAP_TOKEN_RING,	array_length(pkt_tcp) },

	{ "tr",	 "Token-Ring",
		PKT_TR,		NULL,		WTAP_ENCAP_TOKEN_RING,	0 },

	{ "udp", "User Datagram Protocol",
		PKT_UDP,	pkt_udp,	WTAP_ENCAP_ETHERNET,	array_length(pkt_udp) },

};



static int parse_type(char *string);
static void usage(void);
static void seed(void);

static pkt_example* find_example(int type);

int
main(int argc, char **argv)
{

	wtap_dumper		*dump;
	struct wtap_pkthdr	pkthdr;
	union wtap_pseudo_header	ps_header;
	int 			i, j, len_this_pkt, len_random, err;
	guint8			buffer[65536];

	int			opt;
	extern char		*optarg;
	extern int		optind;

	int			produce_count = 1000; /* number of pkts to produce */
	int			produce_type = PKT_ETHERNET;
	char			*produce_filename = NULL;
	int			produce_max_bytes = 5000;
	pkt_example		*example;

	while ((opt = getopt(argc, argv, "b:c:t:")) != -1) {
		switch (opt) {
			case 'b':	/* max bytes */
				produce_max_bytes = atoi(optarg);
				if (produce_max_bytes > 65536) {
					fprintf(stderr,
					    "randpkt: Max bytes is 65536\n");
					exit(1);
				}
				break;

			case 'c':	/* count */
				produce_count = atoi(optarg);
				break;

			case 't':	/* type of packet to produce */
				produce_type = parse_type(optarg);
				break;

			default:
				usage();
				break;
		}
	}

	/* any more command line parameters? */
	if (argc > optind) {
		produce_filename = argv[optind];
	}
	else {
		usage();
	}

	example = find_example(produce_type);

	pkthdr.ts.secs = 0;
	pkthdr.ts.nsecs = 0;
	pkthdr.pkt_encap = example->sample_wtap_encap;

	dump = wtap_dump_open(produce_filename, WTAP_FILE_PCAP,
		example->sample_wtap_encap, produce_max_bytes, FALSE /* compressed */, &err);
	if (!dump) {
		fprintf(stderr,
		    "randpkt: Error writing to %s\n", produce_filename);
		exit(2);
	}

	seed();

	/* reduce max_bytes by # of bytes already in sample */
	if (produce_max_bytes <= example->sample_length) {
		fprintf(stderr,
		    "randpkt: Sample packet length is %d, which is greater than or equal to\n",
		    example->sample_length);
		fprintf(stderr, "your requested max_bytes value of %d\n",
		    produce_max_bytes);
		exit(1);
	}
	else {
		produce_max_bytes -= example->sample_length;
	}

	/* Load the sample into our buffer */
	if (example->sample_buffer)
		memcpy(&buffer[0], example->sample_buffer, example->sample_length);

	/* Produce random packets */
	for (i = 0; i < produce_count; i++) {
		if (produce_max_bytes > 0) {
			len_random = (rand() % produce_max_bytes + 1);
		}
		else {
			len_random = 0;
		}

		len_this_pkt = example->sample_length + len_random;

		pkthdr.caplen = len_this_pkt;
		pkthdr.len = len_this_pkt;
		pkthdr.ts.secs = i; /* just for variety */

		for (j = example->sample_length; j < len_this_pkt; j++) {
			/* Add format strings here and there */
			if ((int) (100.0*rand()/(RAND_MAX+1.0)) < 3 && j < (len_random - 3)) {
				memcpy(&buffer[j], "%s", 3);
				j += 2;
			} else {
				buffer[j] = (rand() % 0x100);
			}
		}

		wtap_dump(dump, &pkthdr, &ps_header, &buffer[0], &err);
	}

	wtap_dump_close(dump, &err);

	return 0;

}

/* Print usage statement and exit program */
static
void usage(void)
{
	int	num_entries = array_length(examples);
	int	i;

	printf("Usage: randpkt [-b maxbytes] [-c count] [-t type] filename\n");
	printf("Default max bytes (per packet) is 5000\n");
	printf("Default count is 1000.\n");
	printf("Types:\n");

	for (i = 0; i < num_entries; i++) {
		printf("\t%s\t%s\n", examples[i].abbrev, examples[i].longname);
	}

	printf("\n");

	exit(0);
}

/* Parse command-line option "type" and return enum type */
static
int parse_type(char *string)
{
	int	num_entries = array_length(examples);
	int	i;

	for (i = 0; i < num_entries; i++) {
		if (strcmp(examples[i].abbrev, string) == 0) {
			return examples[i].produceable_type;
		}
	}

	/* Complain */
	fprintf(stderr, "randpkt: Type %s not known.\n", string);
	exit(1);
}

/* Find pkt_example record and return pointer to it */
static
pkt_example* find_example(int type)
{
	int	num_entries = array_length(examples);
	int	i;

	for (i = 0; i < num_entries; i++) {
		if (examples[i].produceable_type == type) {
			return &examples[i];
		}
	}

	fprintf(stderr,
	    "randpkt: Internal error. Type %d has no entry in examples table.\n",
	    type);
	exit(1);
}

/* Seed the random-number generator */
void
seed(void)
{
	unsigned int	randomness;
	time_t now;
#ifndef _WIN32
	int 		fd;
	ssize_t		ret;

	/*
	 * Assume it's at least worth trying /dev/random on UN*X.
	 * If it doesn't exist, fall back on time().
	 *
	 * XXX - does Windows have a system source of entropy?
	 */
	fd = open("/dev/random", O_RDONLY);
	if (fd == -1) {
		if (errno != ENOENT) {
			fprintf(stderr,
			    "randpkt: Could not open /dev/random for reading: %s\n",
			    strerror(errno));
			exit(2);
		}
		goto fallback;
	}

	ret = read(fd, &randomness, sizeof randomness);
	if (ret == -1) {
		fprintf(stderr,
		    "randpkt: Could not read from /dev/random: %s\n",
		    strerror(errno));
		exit(2);
	}
	if ((size_t)ret != sizeof randomness) {
		fprintf(stderr,
		    "randpkt: Tried to read %lu bytes from /dev/random, got %ld\n",
		    (unsigned long)sizeof randomness, (long)ret);
		exit(2);
	}
	srand(randomness);
	return;

fallback:
#endif
	now = time(NULL);
	randomness = (unsigned int) now;

	srand(randomness);
}
