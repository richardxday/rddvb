
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <map>
#include <vector>

#include <rdlib/StdFile.h>

// CRC32 lookup table for polynomial 0x04c11db7

static uint32_t crc_table[256] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
	0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
	0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
	0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
	0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
	0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
	0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
	0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
	0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
	0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
	0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
	0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
	0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
	0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
	0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
	0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
	0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
	0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
	0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
	0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
	0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4};

uint32_t crc32 (const uint8_t *data, uint_t len)
{
	uint32_t crc = 0xffffffff;
	uint_t   i;

	for (i=0; i<len; i++) {
		crc = (crc << 8) ^ crc_table[((crc >> 24) ^ *data++) & 0xff];
	}
	
	return crc;
}

typedef struct {
	uint_t  pid;
	uint8_t cc;
	bool    valid;
	std::vector<uint8_t> data;
} PACKET;

typedef struct {
	uint_t  event_id;
	AString title;
	AString desc;
	uint_t  running;
} PROGRAMME;

uint64_t GetBits(const uint8_t *p, uint_t& nbit, uint_t nbits)
{
	// 01234567012345670123456701234567 nbit
	// 87654321876543218765432187654321 bi
	uint64_t res = 0;

	while (nbits) {
		uint_t  ii = nbit >> 3, bi = 8 - (nbit & 7);
		uint_t  n  = MIN(bi, nbits);
		uint8_t m  = (uint8_t)((1U << bi) - 1);

		res  <<= n;
		bi    -= n;
		res   |= (p[ii] & m) >> bi;
		
		nbit  += n;
		nbits -= n;
	}

	return res;
}

bool GetBit(const uint8_t *p, uint_t& nbit)
{
	return (GetBits(p, nbit, 1) != 0);
}

void DumpHex(const uint8_t *p, uint_t offset, uint_t nbytes, const AString& indent = "")
{
	uint_t i, j;
	
	for (i = 0; i < nbytes; i += 16) {
		printf("%s0x%04x:", indent.str(), offset + i);
		for (j = 0; (j < 16) && ((i + j) < nbytes); j++) printf(" %02x", (uint_t)p[offset + i + j]);
		for (; j < 16; j++) printf("   ");
		printf(": '");
		for (j = 0; (j < 16) && ((i + j) < nbytes); j++) printf("%c", isprint(p[offset + i + j]) ? (uint_t)p[offset + i + j] : '.');
		for (; j < 16; j++) printf(" ");
		printf("'\n");
	}
}

void DecodePacket(const PACKET& packet, std::map<AString,PROGRAMME>& programmes)
{
	if ((packet.pid == 0x0012) && (packet.data[0] == 0x4e) && (packet.data.size() > 3)) {
		const uint8_t *p = &packet.data[0];
		uint_t nbit = 8 + 4;
		uint_t sect_len = GetBits(p, nbit, 12);
		uint_t len = packet.data.size();

		if (len >= (sect_len + 3)) {
			uint_t current;
			
			len = sect_len + 3;
			
			printf("PID 0x%04x data %u bytes (%u bits):\n", packet.pid, len, len * 8);

			//printf("\n"); DumpHex(p, 0, len); printf("\n");
			
			printf("\tservice_id: %u\n", (uint_t)GetBits(p, nbit, 16));
			printf("\treserved: %u\n", (uint_t)GetBits(p, nbit, 2));
			printf("\tversion_number: %u\n", (uint_t)GetBits(p, nbit, 5));
			printf("\tcurrent_next_indicator: %u\n", current = (uint_t)GetBits(p, nbit, 1));
			printf("\tsection_number: %u\n", (uint_t)GetBits(p, nbit, 8));
			printf("\tlast_section_number: %u\n", (uint_t)GetBits(p, nbit, 8));
			printf("\ttransport_stream_id: %u\n", (uint_t)GetBits(p, nbit, 16));
			printf("\toriginal_network_id: %u\n", (uint_t)GetBits(p, nbit, 16));
			printf("\tsegment_last_section_number: %u\n", (uint_t)GetBits(p, nbit, 8));
			printf("\tlast_table_id: %u\n", (uint_t)GetBits(p, nbit, 8));

			uint_t endbit = (len - 4) << 3;
			while (nbit < endbit) {
				uint_t rst, nbytes, event_id;

				printf("\t\tevent_id: %u\n", event_id = (uint_t)GetBits(p, nbit, 16));
				printf("\t\tstart_time: %010lx\n", (ulong_t)GetBits(p, nbit, 40));
				printf("\t\tduration: %06x\n", (uint_t)GetBits(p, nbit, 24));
				printf("\t\trunning_status: %u\n", rst = (uint_t)GetBits(p, nbit, 3));
				printf("\t\tfree_CA_mode: %u\n", (uint_t)GetBits(p, nbit, 1));
				printf("\t\tdescriptors_loop_length: %u\n", nbytes = (uint_t)GetBits(p, nbit, 12));

				nbytes = MIN(nbytes, len - 4 - (nbit >> 3));
				
				//printf("\n"); DumpHex(p, nbit >> 3, nbytes, "\t\t"); printf("\n");

				uint_t endbit1 = MIN(nbit + (nbytes << 3), endbit);
				while (nbit < endbit1) {
					uint_t desc_id  = (uint_t)GetBits(p, nbit, 8);
					uint_t desc_len = (uint_t)GetBits(p, nbit, 8);

					desc_len = MIN(desc_len, (endbit1 - nbit) >> 3);

					uint_t endbit2 = MIN(nbit + (desc_len << 3), endbit1);

					printf("\t\t\tdescriptor_tag: %02x\n", desc_id);
					printf("\t\t\tdescriptor_length: %u\n", desc_len);
					if (desc_id == 0x4d) {
						AString strings[2];
						uint_t ns = 0;
						
						printf("\t\t\tlanguage: %s\n", AString((const char *)p + (nbit >> 3), 3).str());
						nbit += 3 << 3;

						while (nbit < endbit2) {
							uint_t l = GetBits(p, nbit, 8);
							
							l = MIN(l, (endbit2 - nbit) >> 3);

							if (p[nbit >> 3] == 0x15) {
								nbit += 8;
								l--;
							}
							
							AString text((const char *)p + (nbit >> 3), l);
							printf("\t\t\ttext[%u]: %s\n", l, text.str());
							nbit += l << 3;

							if (ns < NUMBEROF(strings)) strings[ns++] = text;
						}

						if (current && (ns == NUMBEROF(strings))) {
							PROGRAMME& prog = programmes[strings[0]];

							prog.event_id = event_id;
							prog.title    = strings[0];
							prog.desc  	  = strings[1];
							prog.running  = rst;

							printf("Set '%s' to status %u (%u)\n", prog.title.str(), prog.running, prog.event_id);
						}
					}
					else {
						DumpHex(p, nbit >> 3, desc_len, "\t\t\t");
						nbit += desc_len << 3;
					}
				}
			}
		}
		else printf("PID 0x%04x not enough data, claims %u bytes, only got %u bytes\n\n", packet.pid, sect_len, len - 3);
	}
	else printf("PID 0x%04x not interested\n\n", packet.pid);
}

void AddData(PACKET& packet, const uint8_t *p, uint_t len)
{
	if (len) {
		uint_t oldlen = packet.data.size();
		packet.data.resize(oldlen + len);
		memcpy(&packet.data[oldlen], p, len);
	}
}

int main(int argc, char *argv[])
{
	AStdFile fp;

	if (argc < 2) return -1;
	
	if (fp.open(argv[1], "rb")) {
		static uint8_t b[188];
		std::map<uint_t,PACKET>     packets;
		std::map<AString,PROGRAMME> programmes;
		ulong_t filepos = 0;
		uint_t pos = 0, len;
		
		while ((len = fp.readbytes(b + pos, sizeof(b) - pos)) > 0) {
			uint_t i;

			len += pos;
			pos = 0;
			
			for (i = 0; (i < len) && (b[i] != 0x47); i++) ;
			if (i) {
				printf("Lost sync at %lu, losing %u bytes\n", filepos, i);
				
				len -= i;
				if (len) memmove(b, b + i, len);
				pos = len;
			}
			else if (b[0] == 0x47) {
				uint_t   nbit = 8;
				bool     tei  = GetBit(b, nbit);
				bool     pusi = GetBit(b, nbit);
				bool     tp   = GetBit(b, nbit);
				uint_t   pid  = GetBits(b, nbit, 13);
				uint_t   sc   = GetBits(b, nbit, 2);
				bool     af   = GetBit(b, nbit);
				bool     pl   = GetBit(b, nbit);
				uint8_t	 cc   = GetBits(b, nbit, 4);

				(void)tp;

				if		(tei) printf("PID 0x%04x packet is in error!\n", pid);
				else if (sc)  printf("PID 0x%04x packet is scrambled!\n", pid);
				else if (pid) {
					uint_t p = nbit >> 3;
					
					if (af) p = MIN(p + b[p] + 1, len);

					if (pl) {
						PACKET& packet = packets[pid];
						if (pusi) {
							if (packet.data.size()) {
								AddData(packet, b + p + 1, b[p]);
								DecodePacket(packet, programmes);
								packet.data.resize(0);
							}

							p = MIN(p + b[p] + 1, len);

							packet.pid 	 = pid;
							packet.cc  	 = cc;
							packet.valid = true;
							AddData(packet, b + p, len - p);
						}
						else if (packet.valid) {
							uint8_t nextcc = (packet.cc + 1) & 0x0f;

							if (cc != nextcc) printf("PID 0x%04x packet discontinuity (%u)\n", pid, (cc + 16 - nextcc) & 0x0f);

							if (cc == packet.cc) printf("PID 0x%04x packet repeat\n", pid);
							else {
								packet.pid 	 = pid;
								packet.cc  	 = cc;
								packet.valid = true;
								AddData(packet, b + p, len - p);
							}
						}
						//else printf("PID 0x%04x packet, cannot add data\n", pid);
					}
				}
			}
			else printf("Error at %lu (0x%02x)\n", filepos, (uint_t)b[0]);
			
			filepos = fp.tell();
		}
		
		fp.close();
	}
	
	return 0;
}

