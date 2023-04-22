"openocd.exe" -c "gdb_port 50000" -c "tcl_port 50001" -c "telnet_port 50002" ^
    -s "%OPENOCD_PATH%\tcl" -f interface/picoprobe.cfg -f target/rp2040.cfg ^
    -c "adapter speed 5000" -c "program build/examples/tcp_data_transfer/tcp_data_transfer.elf verify reset exit"

@REM build/examples/loopback/w5x00_loopback.elf