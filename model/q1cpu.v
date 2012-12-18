/**
 * @file q1cpu.v
 * @author Joe Wingbermuehle
 * @date 2009-01-01
 *
 * This is a Verilog description of the Q1 CPU. Note that edge-triggered
 * logic is used here instead of a multi-phased clock with latches.
 */

// The Q1 states.
`define STATE_COUNT     8
`define STATE_FETCH1    7
`define STATE_PC1       6
`define STATE_FETCH2    5
`define STATE_PC2       4
`define STATE_FETCH3    3
`define STATE_PC3       2
`define STATE_EX        1
`define STATE_HALT      0

/** Q1 ALU.
 * @param func_in The decoded instruction function.
 * @param a_in The first input (the B register).
 * @param b_in The second input (the C register).
 * @param result_out The result output.
 * @param carry_out The carry.
 * @param zero_out Set if zero.
 * @param neg_out Set if negative.
 */
module q1alu(func_in, a_in, b_in, result_out, carry_out, zero_out, neg_out);

   input wire [8:0] func_in;
   input wire [7:0] a_in;
   input wire [7:0] b_in;
   output reg [7:0] result_out;
   output reg carry_out;
   output wire zero_out;
   output wire neg_out;

   always @(*) begin
      case (1'b1)
         func_in[0]:    // AND
            begin
               result_out <= a_in & b_in;
               carry_out <= 0;
            end
         func_in[1]:    // OR
            begin
               result_out <= a_in | b_in;
               carry_out <= 0;
            end
         func_in[2]:    // SHL
            begin
               result_out <= a_in << 1;
               carry_out <= a_in[7];
            end
         func_in[3]:    // SHR
            begin
               result_out <= a_in >> 1;
               carry_out <= a_in[0];
            end
         func_in[4]:    // ADD
            begin
               {carry_out, result_out} <= {1'b0, a_in} + {1'b0, b_in};
            end
         func_in[5]:    // INC
            begin
               {carry_out, result_out} <= {1'b0, a_in} + 1;
            end
         func_in[6]:    // DEC
            begin
               {carry_out, result_out} <= {1'b0, a_in} - 1;
            end
         func_in[7]:    // NOT
            begin
               result_out <= ~a_in;
               carry_out <= 0;
            end
         default:       // CLR
            begin
               result_out <= 0;
               carry_out <= 0;
            end
      endcase
   end

   assign zero_out = result_out == 0;
   assign neg_out = result_out[7];

endmodule

/** The main Q1 CPU module.
 * @param rst_in Synchronous reset.
 * @param clk_in Clock.
 * @param data_io Data I/O line (8 bits).
 * @param addr_out Address line (16 bits).
 * @param rd_out Memory read.
 * @param wr_out Memory write.
 */
module q1cpu(rst_in, clk_in, data_io, addr_out, rd_out, wr_out);

   input wire rst_in;
   input wire clk_in;
   inout wire [7:0] data_io;
   output wire [15:0] addr_out;
   output wire rd_out;
   output wire wr_out;

   // CPU registers.
   reg [`STATE_COUNT - 1:0] state;
   reg [7:0] regi;
   reg [15:0] rego;
   reg [7:0] rega;
   reg [7:0] regb;
   reg [7:0] regc;
   reg [15:0] regx;
   reg [15:0] regp;
   reg [15:0] regn;
   reg carry_flag;
   reg zero_flag;
   reg neg_flag;

   // Control lines.
   wire take_branch  = (~regi[0] | carry_flag)
                     & (~regi[1] | zero_flag)
                     & (~regi[2] | neg_flag);
   wire rd_a_d = (state[`STATE_EX] & class[1] & func[8])
               | (state[`STATE_EX] & class[3] & func[0])
               | (state[`STATE_EX] & class[3] & func[1])
               | (state[`STATE_EX] & class[3] & func[2]);
   wire wr_a_alu = state[`STATE_EX] & class[2];
   wire rd_b_d = (state[`STATE_EX] & class[1] & func[4])
               | (state[`STATE_EX] & class[3] & func[3]);
   wire wr_b_d = (state[`STATE_EX] & class[1] & func[0])
               | (state[`STATE_EX] & class[3] & func[0])
               | (state[`STATE_EX] & class[3] & func[5]);
   wire rd_c_d = (state[`STATE_EX] & class[1] & func[5])
               | (state[`STATE_EX] & class[3] & func[4]);
   wire wr_c_d = (state[`STATE_EX] & class[1] & func[1])
               | (state[`STATE_EX] & class[3] & func[1])
               | (state[`STATE_EX] & class[3] & func[6]);
   wire rd_xh_d   = state[`STATE_EX] & class[1] & func[6];
   wire wr_xh_d   = state[`STATE_EX] & class[1] & func[2];
   wire rd_xl_d   = state[`STATE_EX] & class[1] & func[7];
   wire wr_xl_d   = state[`STATE_EX] & class[1] & func[3];
   wire rd_x_a = ((state[`STATE_EX] & class[3])
               & (func[2] | func[3] | func[4] | func[5] | func[6]))
               | (state[`STATE_EX] & class[3] & func[7]);
   wire wr_x_a = state[`STATE_PC3] & class[0] & take_branch & regi[3];
   wire rd_p_a = state[`STATE_FETCH1] | state[`STATE_FETCH2]
               | state[`STATE_FETCH3];
   wire wr_p_a = state[`STATE_PC1] | state[`STATE_PC2] | state[`STATE_PC3]
               | (state[`STATE_EX] & class[0] & take_branch)
               | (state[`STATE_EX] & class[3] & func[7]);
   wire rd_n_a = state[`STATE_PC1] | state[`STATE_PC2] | state[`STATE_PC3];
   wire wr_n   = state[`STATE_FETCH1] | state[`STATE_FETCH2]
               | state[`STATE_FETCH3];
   wire wr_i_d = state[`STATE_FETCH1];
   wire rd_o_a = (state[`STATE_EX] & class[0])
               | (state[`STATE_EX] & class[1]);
   wire wr_oh_d = state[`STATE_FETCH2];
   wire wr_ol_d = state[`STATE_FETCH3];
   wire mem_rd = state[`STATE_FETCH1] | state[`STATE_FETCH2]
               | state[`STATE_FETCH3]
               | (state[`STATE_EX] & class[1]
                  & (func[0] | func[1] | func[2] | func[3]))
               | (state[`STATE_EX] & class[3] & func[5])
               | (state[`STATE_EX] & class[3] & func[6]);
   wire mem_wr = (state[`STATE_EX] & class[1]
                  & (func[4] | func[5] | func[6] | func[7] | func[8]))
               | (state[`STATE_EX] & class[3]
                  & (func[2] | func[3] | func[4]));

   // Register I (instruction).
   always @(posedge clk_in) begin
      if (wr_i_d) regi <= data_io;
   end

   // Register O (opcode).
   always @(posedge clk_in) begin
      if (wr_oh_d)      rego[15:8] <= data_io;
      else if (wr_ol_d) rego[7:0] <= data_io;
   end

   // Decode the instruction class.
   reg [3:0] class;
   always @(*) begin : class_decoder
      integer x;
      for(x = 0; x < 4; x = x + 1) begin
         class[x] = regi[7:4] == x;
      end
   end

   // Decode the instruction function.
   reg [8:0] func;
   always @(*) begin : func_decoder
      integer x;
      for(x = 0; x < 9; x = x + 1) begin
         func[x] = regi[3:0] == x;
      end
   end

   // State logic
   wire has_operand = class[0] | class[1];
   wire is_halt = class[3] & func[8];
   always @(posedge clk_in) begin
      if (rst_in) begin
         state <= 1 << `STATE_FETCH1;
      end else begin
         case (1'b1)
            state[`STATE_FETCH1]:
               state <= 1 << `STATE_PC1;
            state[`STATE_PC1]:
               state <= has_operand ? (1 << `STATE_FETCH2) : (1 << `STATE_EX);
            state[`STATE_FETCH2]:
               state <= 1 << `STATE_PC2;
            state[`STATE_PC2]:
               state <= 1 << `STATE_FETCH3;
            state[`STATE_FETCH3]:
               state <= 1 << `STATE_PC3;
            state[`STATE_PC3]:
               state <= 1 << `STATE_EX;
            state[`STATE_EX]:
               state <= is_halt ? (1 << `STATE_HALT) : (1 << `STATE_FETCH1);
            state[`STATE_HALT]:
               state <= 1 << `STATE_HALT;
            default:
               state <= 0;
         endcase
      end
   end

   // ALU.
   wire [7:0] alu_out;
   wire carry_out;
   wire zero_out;
   wire neg_out;
   q1alu alu(func, regb, regc, alu_out, carry_out, zero_out, neg_out);

   // Register A.
   always @(posedge clk_in) begin
      if (wr_a_alu) begin
         rega <= alu_out;
         carry_flag <= carry_out;
         zero_flag <= zero_out;
         neg_flag <= neg_out;
      end
   end

   // Register B.
   always @(posedge clk_in) begin
      if (wr_b_d) regb <= data_io;
   end

   // Register C.
   always @(posedge clk_in) begin
      if (wr_c_d) regc <= data_io;
   end

   // Register X.
   always @(posedge clk_in) begin
      if (wr_xh_d)      regx[15:8] <= data_io;
      else if (wr_xl_d) regx[7:0] <= data_io;
      else if (wr_x_a)  regx <= addr_out;
   end

   // Register P.
   always @(posedge clk_in) begin
      if (rst_in) begin
         regp <= 0;
      end else begin
         if (wr_p_a)    regp <= addr_out;
      end
   end

   // Register N.
   always @(posedge clk_in) begin
      if (wr_n) regn <= addr_out + 1;
   end

   // Data I/O.
   assign data_io = rd_a_d ? rega : 8'bz;
   assign data_io = rd_b_d ? regb : 8'bz;
   assign data_io = rd_c_d ? regc : 8'bz;
   assign data_io = rd_xh_d ? regx[15:8] : 8'bz;
   assign data_io = rd_xl_d ? regx[7:0] : 8'bz;

   // Memory read/write.
   assign wr_out = mem_wr;
   assign rd_out = mem_rd;

   // Address line.
   assign addr_out = rd_x_a ? regx : 16'bz;
   assign addr_out = rd_p_a ? regp : 16'bz;
   assign addr_out = rd_n_a ? regn : 16'bz;
   assign addr_out = rd_o_a ? rego : 16'bz;

endmodule

