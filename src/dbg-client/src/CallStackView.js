import m from 'mithril';

export const CallStackView = {
	view: (vnode) => {
		return m("div.pure-menu", [
			m("span.pure-menu-heading", "Call Stack"),
			m("ul.pure-menu-list", vnode.attrs.callStack.map((entry) =>
				m("li.pure-menu-item", [
					m("a.pure-menu-link",
						{ onclick: () => vnode.attrs.stackLevel(entry) },
						entry.filename
					)
				])
			))
		]);
	}
};
