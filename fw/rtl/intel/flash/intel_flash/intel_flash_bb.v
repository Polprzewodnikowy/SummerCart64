
module intel_flash (
	clock,
	avmm_data_addr,
	avmm_data_read,
	avmm_data_readdata,
	avmm_data_waitrequest,
	avmm_data_readdatavalid,
	avmm_data_burstcount,
	reset_n);	

	input		clock;
	input	[15:0]	avmm_data_addr;
	input		avmm_data_read;
	output	[31:0]	avmm_data_readdata;
	output		avmm_data_waitrequest;
	output		avmm_data_readdatavalid;
	input	[1:0]	avmm_data_burstcount;
	input		reset_n;
endmodule
