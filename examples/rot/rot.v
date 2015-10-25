
module top(input clk, output D1, output D2, output D3, output D4, output D5);
   
   reg ready = 0;
   reg [23:0]     divider;
   reg [3:0]      rot;
   
   always @(posedge clk) begin
      if (ready) 
        begin
           if (divider == 12000000) 
             begin
                divider <= 0;
                rot <= {rot[2:0], rot[3]};
             end
           else 
             divider <= divider + 1;
        end
      else 
        begin
           ready <= 1;
           rot <= 4'b0001;
           divider <= 0;
        end
   end
   
   assign D1 = rot[0];
   assign D2 = rot[1];
   assign D3 = rot[2];
   assign D4 = rot[3];
   assign D5 = 1;
endmodule // top
