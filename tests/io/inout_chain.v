
module chip (output io_0_3_0, output io_0_3_1, input io_0_4_0);

wire io_0_3_0;
assign io_0_3_1 = io_0_3_0;
assign io_0_3_0 = io_0_4_0;


endmodule

