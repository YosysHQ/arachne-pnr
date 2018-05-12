
module chip (input io_0_8_0, input io_0_9_0, output io_0_9_1, input io_0_10_0, input io_0_10_1);

wire io_0_8_0, n2, io_0_9_0, n4, io_0_9_1, io_0_10_0, io_0_10_1, n8, n9;

// IO Cell (0, 9, 1)
assign io_0_9_1 = n4 ? n2 : 1'bz;

assign n8 = /* LUT    1  9  3 */ 1'b0 ? 1'b0 : io_0_10_1 ? io_0_8_0 ? 1'b1 : 1'b0 : io_0_9_0 ? 1'b1 : 1'b0;
assign n9 = /* LUT    1 10  5 */ 1'b0 ? 1'b0 : 1'b0 ? 1'b0 : io_0_10_1 ? io_0_10_0 ? 1'b1 : 1'b0 : 1'b1;
/* FF  1  9  3 */ assign n2 = n8;
/* FF  1 10  5 */ assign n4 = n9;

endmodule

