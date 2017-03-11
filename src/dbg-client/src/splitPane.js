import h from 'snabbdom/h';
import Split from 'split.js';
import './splitPane.scss';

export const splitPane = (opts, children) => {
	const childElms = new Array(children.length);
	let count = 0;

	return h("div.full-height",
		children.map((child, index) => h("div.split", {
			class: { ["split-" + opts.direction]: true },
			hook: {
				insert: (vnode) => {
					childElms[index] = vnode.elm;
					if(++count == childElms.length) {
						Split(childElms, opts);
					}
				}
			}
		}, [child]))
	);
}
