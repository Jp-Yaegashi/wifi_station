
<a target="_blank" 
    href="./img/1.jpg">
    <img style="max-width:100%;" alt="IO"
        src="./img/1.jpg">
</a>

### ビルドコマンド
```
west build -b nrf7002dk/nrf5340/cpuapp -p always --no-sysbuild -- \ 
  -DEXTRA_DTC_OVERLAY_FILE=boards/nrf7002dk_nrf5340_cpuapp.overlay
```

### 書き込みコマンド
```
west flash --runner nrfjprog
```
### トークン
```
eyJhbGciOiJIUzI1NiJ9.eyJpc3MiOiJodHRwczovL2FwaS5zdGctbmV3c2VkdGVjaC1pb3Qub25taXJhLmNsb3VkLyIsInN1YiI6ImFjY2VzcyIsImF1ZCI6InN0Zl9VV2Y5OFhFSWd1YVNMOCIsImp0aSI6IjI0NTk2YjRlLWM0ZGUtNGZlZi04NzU0LTE5ODIzYzQ0OGFkNyIsImlhdCI6MTczNDMzNTM5NH0.D22HW9P4Hsvjdy7oMgcxFi3215R89Q4fkVxfJt4ERko
```