import { parse as parseURITemplate } from 'uri-template';

export class HALApp {
	constructor(opts) {
		this.custom_rels = opts.custom_rels;
		this.codecs = opts.codecs;

		const a = document.createElement('a');
		a.href = opts.entrypoint;
		this.baseURL = a.protocol + '//' + a.hostname + ':' + a.port;

		this.entrypoint = new HALLink({href: opts.entrypoint}, this);
	}

	fetch() {
		return this.entrypoint.fetch();
	}
}

class HALResource {
	constructor(doc, app, curies_) {
		Object.assign(this, doc);

		const links = doc._links || {}
		const embedded = doc._embedded || {}

		const curies = curies_ || {};
		(links.curies || []).forEach(curie => {
			curies[curie.name] = parseURITemplate(curie.href);
		});

		mapToObj(links, doc => new HALLink(doc, app));
		mapToObj(embedded, doc => new HALResource(doc, app, curies));

		expandCuries(links, curies);
		expandCuries(embedded, curies);

		this._links = links;
		this._embedded = embedded;
		this._app = app;
	}

	getLink(rel) {
		if(rel in this._app.custom_rels) {
			return this.getLink(this._app.custom_rels[rel]);
		} else {
			return this._links[rel];
		}
	}

	getEmbedded(rel) {
		if(rel in this._app.custom_rels) {
			return this.getEmbedded(this._app.custom_rels[rel]);
		} else {
			return this._embedded[rel];
		}
	}
}

class HALLink {
	constructor(link, app) {
		Object.assign(this, link);
		const href = link.href;
		if(href.startsWith('/') && !href.startsWith('//')) {
			this.href = app.baseURL + link.href;
		}

		this._app = app;
	}

	fetch(opts) {
		return fetch(this.href, opts)
			.then(resp => {
				if(!resp.ok) { return Promise.reject(resp); }
				if(!resp.headers.has("Content-Type")) { return Promise.reject(resp); }

				const codecs = this._app.codecs;
				const contentType = resp.headers.get("Content-Type");
				if(contentType in codecs) {
					const codec = codecs[contentType];
					return codec
						.decode(resp)
						.then(doc => codec.isHAL ? new HALResource(doc, this._app) : doc);
				} else {
					return resp;
				}
			});
	}
}

function mapToObj(collection, fn) {
	for(let rel in collection) {
		const item = collection[rel];
		if(Array.isArray(item)) {
			for(let index in item) {
				item[index] = fn(item[index]);
			}
		} else {
			collection[rel] = fn(item);
		}
	}
}

function expandCuries(links, curies) {
	for(let curie in curies) {
		for(let rel in links) {
			if(rel.startsWith(curie + ":")) {
				let shortRelName = rel.substring(curie.length + 1);
				let fullLinkRel = curies[curie].expand({rel: shortRelName});
				links[fullLinkRel] = links[rel];
			}
		}
	}
}
