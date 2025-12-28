ビルドコマンド

west build -b nrf7002dk/nrf5340/cpuapp -p always --no-sysbuild -- \ 
  -DEXTRA_DTC_OVERLAY_FILE=boards/nrf7002dk_nrf5340_cpuapp.overlay

書き込みコマンド

west flash --runner nrfjprog