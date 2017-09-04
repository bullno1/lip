gen: bundle.js
	mkdir -p $@
	./incbin.bat bundle.js gen/bundle.js.h bundle_js
	./incbin.bat bundle.js.map gen/bundle.js.map.h bundle_js_map
	./incbin.bat index.html gen/index.html.h index_html

bundle.js: package.json ! live
	npm install
	npm run build
