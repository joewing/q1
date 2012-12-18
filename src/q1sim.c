/* Simulator for the Q1 Computer.
 * Joe Wingbermuehle
 * 20080528
 *
 * To compile:
 *    gcc -O2 -Wall -o q1sim q1sim.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Size of the hex dump to display. */
#define MAX_LINES 24
#define BYTES_PER_LINE (8 * 2)

/* Registers and memory. */
static unsigned char rega, regb, regc;
static unsigned char z_flag, c_flag, n_flag;
static unsigned char regxh, regxl;
static unsigned short preg;
static unsigned char halted;
static unsigned char opcode;
static unsigned short operand;
static unsigned char memory[1 << 16];

static unsigned int clocks;

static void DisplayState();

static void ldb() {
   regb = memory[operand];
}

static void ldc() {
   regc = memory[operand];
}

static void lxh() {
   regxh = memory[operand];
}

static void lxl() {
   regxl = memory[operand];
}

static void stb() {
   memory[operand] = regb;
}

static void stc() {
   memory[operand] = regc;
}

static void sxh() {
   memory[operand] = regxh;
}

static void sxl() {
   memory[operand] = regxl;
}

static void sta() {
   memory[operand] = rega;
}

static void and() {
   rega = regb & regc;
   c_flag = 0;
   z_flag = rega == 0;
   n_flag = rega >> 7;
}

static void or() {
   rega = regb | regc;
   c_flag = 0;
   z_flag = rega == 0;
   n_flag = rega >> 7;
}

static void shl() {
   rega = regb << 1;
   c_flag = regb >> 7;
   z_flag = rega == 0;
   n_flag = rega >> 7;
}

static void shr() {
   rega = regb >> 1;
   c_flag = regb & 1;
   z_flag = rega == 0;
   n_flag = rega >> 7;
}

static void add() {
   unsigned short temp = regb + regc;
   rega = (unsigned char)temp;
   c_flag = temp > 255;
   z_flag = rega == 0;
   n_flag = rega >> 7;
}

static void inc() {
   rega = regb + 1;
   c_flag = regb == 255;
   z_flag = rega == 0;
   n_flag = rega >> 7;
}

static void dec() {
   rega = regb - 1;
   c_flag = regb == 0;
   z_flag = rega == 0;
   n_flag = rega >> 7;
}

static void not() {
   rega = ~regb;
   c_flag = 0;
   z_flag = rega == 0;
   n_flag = rega >> 7;
}

static void clr() {
   rega = 0;
   c_flag = 0;
   z_flag = 1;
   n_flag = 0;
}

static void mab() {
   regb = rega;
}

static void mac() {
   regc = rega;
}

static void sax() {
   memory[(regxh << 8) | regxl] = rega;
}

static void sbx() {
   memory[(regxh << 8) | regxl] = regb;
}

static void scx() {
   memory[(regxh << 8) | regxl] = regc;
}

static void lbx() {
   regb = memory[(regxh << 8) | regxl];
}

static void lcx() {
   regc = memory[(regxh << 8) | regxl];
}

static void ret() {
   preg = (regxh << 8) | regxl;
}

static void hlt() {
   halted = 1;
}

typedef void (*InstructionFunc)();

static InstructionFunc ls_class[16] = {
   ldb, ldc, lxh, lxl, stb, stc, sxh, sxl, sta
};

static InstructionFunc math_class[16] = {
   and, or, shl, shr, add, inc, dec, not, clr
};

static InstructionFunc misc_class[16] = {
   mab, mac, sax, sbx, scx, lbx, lcx, ret, hlt
};

static void j_inst(unsigned char func) {

   const unsigned char c_func = (func >> 0) & 1;
   const unsigned char z_func = (func >> 1) & 1;
   const unsigned char n_func = (func >> 2) & 1;
   const unsigned char is_call = (func >> 3) & 1;

   operand = (unsigned short)memory[preg++] << 8;
   operand |= memory[preg++];

   if((!c_func | c_flag) & (!z_func | z_flag) & (!n_func | n_flag)) {
      if(is_call) {
         regxh = preg >> 8;
         regxl = preg & 0xFF;
      }
      preg = operand;
   }
}

static void ls_inst(unsigned char func) {
   void (*inst)() = ls_class[func];
   operand = (unsigned short)memory[preg++] << 8;
   operand |= memory[preg++];
   if(inst) {
      (inst)();
   } else {
      fprintf(stderr, "ERROR: invalid LS instruction: %u\n",
         (unsigned int)func);
   }
}

static void math_inst(unsigned char func) {
   void (*inst)() = math_class[func];
   if(inst) {
      (inst)();
   } else {
      fprintf(stderr, "ERROR: invalid MATH instruction: %u\n",
         (unsigned int)func);
   }
}

static void misc_inst(unsigned char func) {
   void (*inst)() = misc_class[func];
   if(inst) {
      (inst)();
   } else {
      fprintf(stderr, "ERROR: invalid MISC instruction: %u\n",
         (unsigned int)func);
   }
}

/* Execute the next instruction. */
static void next() {

   opcode = memory[preg];
   const unsigned char inst_class = opcode >> 4;
   const unsigned char inst_func = opcode & 0x0F;

   ++preg;

   switch(inst_class) {
   case 0:     // Jump
      j_inst(inst_func);
      clocks += 7 * 3;
      break;
   case 1:     // Load/Store
      ls_inst(inst_func);
      clocks += 7 * 3;
      break;
   case 2:     // Math
      math_inst(inst_func);
      clocks += 3 * 3;
      break;
   case 3:     // Misc
      misc_inst(inst_func);
      clocks += 3 * 3;
      break;
   default:
      fprintf(stderr, "ERROR: invalid instruction class: %u\n",
         (unsigned int)inst_class);
      break;
   }

}

int main(int argc, char *argv[]) {

   FILE *fd;
   const char *file_name = NULL;
   int x;

   rega = 0xFF;
   regb = 0xFF;
   regc = 0xFF;
   regxh = 0xFF;
   regxl = 0xFF;
   c_flag = 1;
   z_flag = 1;
   n_flag = 1;

   for(x = 1; x < argc; x++) {
      if(!strcmp(argv[x], "-a") && x + 1 < argc) {
         ++x;
         rega = (unsigned char)atoi(argv[x]);
      } else if(!strcmp(argv[x], "-b") && x + 1 < argc) {
         ++x;
         regb = (unsigned char)atoi(argv[x]);
      } else if(!strcmp(argv[x], "-c") && x + 1 < argc) {
         ++x;
         regc = (unsigned char)atoi(argv[x]);
      } else if(!strcmp(argv[x], "-h") || file_name != NULL) {
         if(strcmp(argv[x], "-h")) {
            fprintf(stderr, "ERROR: invalid or incomplete argument: %s\n",
               argv[x]);
         }
         fprintf(stderr, "usage: %s [options] <filename>\n", argv[0]);
         fprintf(stderr, "options:\n");
         fprintf(stderr, "\t-a <number>\tValue for register A\n");
         fprintf(stderr, "\t-b <number>\tValue for register B\n");
         fprintf(stderr, "\t-c <number>\tValue for register C\n");
         fprintf(stderr, "\t-h\t\tDisplay this message\n");
         return -1;
      } else {
         file_name = argv[x];
      }
   }

   if(file_name == NULL) {
      fprintf(stderr, "ERROR: no file specified\n");
      return -1;
   }

   fd = fopen(file_name, "rb");
   if(fd == NULL) {
      fprintf(stderr, "ERROR: could not open %s\n", file_name);
      return -1;
   }

   memset(memory, 0xFF, sizeof(memory));

   preg = 0;
   for(;;) {
      x = fgetc(fd);
      if(x == EOF) {
         break;
      }
      if(preg == 0xFFFF) {
         fprintf(stderr, "WARN: input file too large\n");
         break;
      }
      memory[preg++] = (unsigned char)x;
   }

   fclose(fd);

   clocks = 0;
   preg = 0;
   halted = 0;
   while(!halted) {
      DisplayState();
      next();
   }

   return 0;

}

static void DisplayByte(unsigned char byte) {
   int x;
   for(x = 0; x < 8; x++) {
      if(byte & (1 << (7 - x))) {
         printf("o");
      } else {
         printf("-");
      }
   }
}

static void DisplayWord(unsigned short word) {
   DisplayByte((unsigned char)(word >> 8));
   printf(" ");
   DisplayByte((unsigned char)word);
}

void DisplayState() {

   unsigned int x, y;

   printf("\033[2J\033[;H");
   printf("CLOCKS: %u\n", clocks);

   printf("PC: "); DisplayWord(preg);
   printf("       %u\n", (unsigned int)preg);

   printf("A:           "); DisplayByte(rega);
   printf(c_flag ? " C " : "   ");
   printf(z_flag ? "Z " : "  ");
   printf(n_flag ? "N " : "  ");
   printf("%u\n", (unsigned int)rega);

   printf("B:           "); DisplayByte(regb);
   printf("       %u\n", (unsigned int)regb);

   printf("C:           "); DisplayByte(regc);
   printf("       %u\n", (unsigned int)regc);

   printf("X:  "); DisplayByte(regxh); printf(" ");
                   DisplayByte(regxl);
   printf("       %u\n", ((unsigned int)regxh << 8) | regxl);

   x = 0;
   while(x < BYTES_PER_LINE * (MAX_LINES - 9)) {
      printf("%04x:", x);
      for(y = 0; y < BYTES_PER_LINE; y++, x++) {
         if((y & 7) == 0) {
            printf(" ");
         }
         printf("%02x ", (unsigned int)memory[x]);
      }
      printf("\n");
   }

   usleep(100000);

}

