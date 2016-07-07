/*****************************************************************************
 * decode_sdt.c: SDT decoder example
 *----------------------------------------------------------------------------
 * Copyright (C) 2001-2011 VideoLAN
 * $Id: decode_sdt.c,v 1.1 2002/12/11 13:04:56 jobi Exp $
 *
 * Authors: Arnaud de Bossoreille de Ribou <bozo@via.ecp.fr>
 *          Johan Bilien <jobi@via.ecp.fr>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *----------------------------------------------------------------------------
 *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#include <rdlib/StdData.h>
#include <rdlib/StdFile.h>

#include <dvbpsi/dvbpsi.h>
#include <dvbpsi/psi.h>
#include <dvbpsi/demux.h>
#include <dvbpsi/descriptor.h>
#include <dvbpsi/sdt.h>
#include <dvbpsi/eit.h>

/*****************************************************************************
 * ReadPacket
 *****************************************************************************/
static bool ReadPacket(AStdData& fp, uint8_t* p_dst)
{
	const uint_t len = 188;
	uint_t   pos = 0;
	sint32_t bytes;

	while ((bytes = fp.readbytes(p_dst + pos, len - pos)) > 0) {
		uint_t i;

		pos += (uint_t)bytes;
		for (i = 0; (i < pos) && (p_dst[i] != 0x47); i++) ;

		if (i > 0) {
			pos -= i;
			if (pos) memmove(p_dst, p_dst + i, pos);
		}

		if (pos >= len) {
			memcpy(p_dst, p_dst, len);

			pos -= len;
			if (pos) memmove(p_dst, p_dst + len, pos);
			break;
		}
	}

	return (bytes > 0);
}


/*****************************************************************************
 * DumpDescriptors
 *****************************************************************************/
static void DumpDescriptors(const char *str, const dvbpsi_descriptor_t* p_descriptor)
{
	while (p_descriptor)
	{
		int i;
		printf("%s 0x%02x : \"", str, (int)p_descriptor->i_tag);
		for(i = 0; i < p_descriptor->i_length; i++) {
			if (isprint(p_descriptor->p_data[i])) printf("%c", (int)p_descriptor->p_data[i]);
			else								  printf("[%02x]", (int)p_descriptor->p_data[i]);
		}
		printf("\"\n");
		p_descriptor = p_descriptor->p_next;
	}
};

/*****************************************************************************
 * DVBPSI messaging callback
 *****************************************************************************/
static void message(dvbpsi_t *handle, const dvbpsi_msg_level_t level, const char *msg)
{
	(void)handle;
    switch(level)
    {
        case DVBPSI_MSG_ERROR: fprintf(stderr, "Error: "); break;
        case DVBPSI_MSG_WARN:  fprintf(stderr, "Warning: "); break;
        case DVBPSI_MSG_DEBUG: fprintf(stderr, "Debug: "); break;
        default: /* do nothing */
            return;
    }
    fprintf(stderr, "%s\n", msg);
}

static void dumpeit(void *p_cb_data, dvbpsi_eit_t *p_new_eit)
{
	(void)p_cb_data;

	const dvbpsi_eit_event_t *p_event = p_new_eit->p_first_event;
	while (p_event) {
		printf("Event: TS ID %u Network ID %u Event ID %u starttime %s duration %u\n", p_new_eit->i_ts_id, p_new_eit->i_network_id, p_event->i_event_id, AValue(p_event->i_start_time).ToString("016x").str(), p_event->i_duration);

		DumpDescriptors("Event", p_event->p_first_descriptor);

		p_event = p_event->p_next;
	}
}

/*****************************************************************************
 * newsubtable
 *****************************************************************************/
static void newsubtable(dvbpsi_t *p_dvbpsi, uint8_t table_id, uint16_t extension, void *p_zero)
{
	(void)p_zero;
	
	if ((table_id >= 0x4e) && (table_id <= 0x5f))
	{
		if (!dvbpsi_eit_attach(p_dvbpsi, table_id, extension, dumpeit, NULL))
			fprintf(stderr, "Failed to attach SDT subdecoder\n");
	}
}

/*****************************************************************************
 * main
 *****************************************************************************/
int main(int argc, char *argv[])
{
	AStdData *fp = Stdin;
	AStdFile _fp;
	uint8_t  data[188];
	dvbpsi_t *p_dvbpsi;
	bool b_ok;

	(void)DumpDescriptors;
	
	if (argc > 1) {
		if (_fp.open(argv[1], "rb")) fp = &_fp;
		else {
			fprintf(stderr, "Failed to open file '%s' for reading\n", argv[1]);
			exit(1);
		}
	}
	
	p_dvbpsi = dvbpsi_new(&message, DVBPSI_MSG_WARN);
	if (p_dvbpsi) {
		if (dvbpsi_AttachDemux(p_dvbpsi, newsubtable, NULL)) {
			b_ok = ReadPacket(*fp, data);
			
			while (b_ok)
			{
				uint16_t pid = ((uint16_t)(data[1] & 0x1f) << 8) + data[2];
				if (pid == 0x12) dvbpsi_packet_push(p_dvbpsi, data);
				b_ok = ReadPacket(*fp, data);
			}
			dvbpsi_DetachDemux(p_dvbpsi);
		}
		else fprintf(stderr, "Failed to attach demux to dvbpsi handler\n");
			
		dvbpsi_delete(p_dvbpsi);
	}
	else fprintf(stderr, "Failed to create new dvbpsi handler\n");

	_fp.close();
	
	return 0;
}
