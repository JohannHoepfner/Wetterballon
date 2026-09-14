.DELETE_ON_ERROR:

all: gps.csv geiger.csv pt1000.csv

DATA: DATA.sens_misc DATA.wwan_gateway
	sort $^ -n | uniq | grep -E '^[2-4][0-9]{4}:' | grep -v 'invalid' > $@

gps.csv: DATA
	sed -n 's/^\([0-9]*\):lat=\([0-9]*\.[0-9]*\),lon=\([0-9]*\.[0-9]*\),alt=\([0-9]*\.[0-9]*\),utc=\([0-9]*:[0-9]*:[0-9]*\)\.[0-9]*/\1,\5,\2,\3,\4/p' $^ > $@

geiger.csv: DATA
	sed -n 's/^\([0-9]*\):s=\([0-9]\)/\1,\2/p' $^ > $@

pt1000.csv: DATA
	sed -n 's/^\([0-9]*\):a0-a1=\([0-9]*\.[0-9]*\),a3=\([0-9]*\.[0-9]*\),t1=\(-*[0-9]*\.[0-9]*\)/\1,\2,\3,\4/p' $^ > $@
