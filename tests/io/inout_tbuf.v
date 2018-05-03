
module chip (input io_0_13_1, input io_0_12_1, output io_0_13_0);

wire io_0_13_1, io_0_12_1, io_0_13_0;

// IO Cell (0, 13, 0)
assign io_0_13_0 = io_0_12_1 ? io_0_13_1 : 1'bz;


endmodule

