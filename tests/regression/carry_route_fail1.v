module test(input i0, input i2, input i4, input i5, input i6, input i7, input i8, input i9, input i13, input i14, input i15, input i16, input i17, input i18, input i19, input i20, input i22, input i23, input i24, input i25, input i27, input i28, input i29, input i31, input i32, input i34, input i35, input i36, input i37, input i38, input i39, input i40, input i41, output o0);
  wire [9:0] t7 = {i24, i4, i29, i13, i0, i35, i41, i14, i38, i7} + {i40, i37, i2, i18, i39, i36, i15, i25, i31, i16};
  assign o0 = t7[9];
endmodule
