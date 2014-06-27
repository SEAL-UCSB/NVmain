
/****************************************************************************************
*
*    File Name:  subtest.vh
*
*  Description:  Micron SDRAM DDR3 (Double Data Rate 3)
*                This file is included by tb.v
*                Modified for NVMain verification. This file includes an nvmain verilog
*                trace named "nvmain_trace.vh"
*
*   Disclaimer   This software code and all associated documentation, comments or other 
*  of Warranty:  information (collectively "Software") is provided "AS IS" without 
*                warranty of any kind. MICRON TECHNOLOGY, INC. ("MTI") EXPRESSLY 
*                DISCLAIMS ALL WARRANTIES EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED 
*                TO, NONINFRINGEMENT OF THIRD PARTY RIGHTS, AND ANY IMPLIED WARRANTIES 
*                OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE. MTI DOES NOT 
*                WARRANT THAT THE SOFTWARE WILL MEET YOUR REQUIREMENTS, OR THAT THE 
*                OPERATION OF THE SOFTWARE WILL BE UNINTERRUPTED OR ERROR-FREE. 
*                FURTHERMORE, MTI DOES NOT MAKE ANY REPRESENTATIONS REGARDING THE USE OR 
*                THE RESULTS OF THE USE OF THE SOFTWARE IN TERMS OF ITS CORRECTNESS, 
*                ACCURACY, RELIABILITY, OR OTHERWISE. THE ENTIRE RISK ARISING OUT OF USE 
*                OR PERFORMANCE OF THE SOFTWARE REMAINS WITH YOU. IN NO EVENT SHALL MTI, 
*                ITS AFFILIATED COMPANIES OR THEIR SUPPLIERS BE LIABLE FOR ANY DIRECT, 
*                INDIRECT, CONSEQUENTIAL, INCIDENTAL, OR SPECIAL DAMAGES (INCLUDING, 
*                WITHOUT LIMITATION, DAMAGES FOR LOSS OF PROFITS, BUSINESS INTERRUPTION, 
*                OR LOSS OF INFORMATION) ARISING OUT OF YOUR USE OF OR INABILITY TO USE 
*                THE SOFTWARE, EVEN IF MTI HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH 
*                DAMAGES. Because some jurisdictions prohibit the exclusion or 
*                limitation of liability for consequential or incidental damages, the 
*                above limitation may not apply to you.
*
*                Copyright 2003 Micron Technology, Inc. All rights reserved.
*
****************************************************************************************/

    initial begin : test
        parameter [31:0] REP = DQ_BITS/8.0;

        reg         [BA_BITS-1:0] r_bank;
        reg        [ROW_BITS-1:0] r_row;
        reg        [COL_BITS-1:0] r_col;
        reg  [BL_MAX*DQ_BITS-1:0] r_data;
        integer                   r_i, r_j;


        real original_tck;
        
        rst_n   <=  1'b0;
        cke     <=  1'b0;
        cs_n    <=  1'b1;
        ras_n   <=  1'b1;
        cas_n   <=  1'b1;
        we_n    <=  1'b1;
        ba      <=  {BA_BITS{1'bz}};
        a       <=  {ADDR_BITS{1'bz}};
        odt_out <=  1'b0;
        dq_en   <=  1'b0;
        dqs_en  <=  1'b0;
        
        // POWERUP SECTION 
        power_up;

        // INITIALIZE SECTION
        zq_calibration  (1);                            // perform Long ZQ Calibration

        load_mode       (3, 14'b00000000000000);        // Extended Mode Register (3)
        nop             (tmrd-1);
        
        load_mode       (2, {14'b00001000_000_000} | mr_cwl<<3); // Extended Mode Register 2 with DCC Disable
        nop             (tmrd-1);
        
        load_mode       (1, 14'b0000010110);            // Extended Mode Register with DLL Enable, AL=CL-1
        nop             (tmrd-1);
        
        load_mode       (0, {14'b0_0_000_1_0_000_1_0_00} | mr_wr<<9 | mr_cl<<2); // Mode Register with DLL Reset

        nop             (tdllk);
        odt_out         <= 1;                           // turn on odt


        $display("%m at time %t: INFO: Reading NVMain Verilog trace", $time);

        nop (10);


        `include "nvmain_trace.vh"


        nop (20);



        test_done;
    end
