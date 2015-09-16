`timescale 1 ns / 1 ps

`default_nettype none

module top(input oscillator, output D1, output D2, output D3, output D4, output D5);

  wire clk;

  // assign clk = oscillator; // This works
  SB_GB clockbuffer ( .USER_SIGNAL_TO_GLOBAL_BUFFER (oscillator), .GLOBAL_BUFFER_OUTPUT (clk) );

  reg [25:0] ticks;
  always @(posedge clk)
    ticks <= ticks + 16'd1;

  assign {D1,D2,D3,D4,D5} = ticks[25:21];

endmodule
