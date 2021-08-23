	intel_flash u0 (
		.clock                   (<connected-to-clock>),                   //    clk.clk
		.avmm_data_addr          (<connected-to-avmm_data_addr>),          //   data.address
		.avmm_data_read          (<connected-to-avmm_data_read>),          //       .read
		.avmm_data_readdata      (<connected-to-avmm_data_readdata>),      //       .readdata
		.avmm_data_waitrequest   (<connected-to-avmm_data_waitrequest>),   //       .waitrequest
		.avmm_data_readdatavalid (<connected-to-avmm_data_readdatavalid>), //       .readdatavalid
		.avmm_data_burstcount    (<connected-to-avmm_data_burstcount>),    //       .burstcount
		.reset_n                 (<connected-to-reset_n>)                  // nreset.reset_n
	);

