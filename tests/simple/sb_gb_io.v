
module test(input inclk, output outclk, output [3:0] out);
   
   SB_GB_IO
     #(.PIN_TYPE(6'b000001))
   gb_io_inst
     (.PACKAGE_PIN(inclk),
      .D_IN_0(outclk),
      .GLOBAL_BUFFER_OUTPUT(clk));
   
   reg [3:0] r;
   assign out = r;
   
   always @(posedge clk) begin
      r <= r + 1;
   end
   
endmodule
