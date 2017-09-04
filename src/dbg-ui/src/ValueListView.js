import h from 'snabbdom/h';
import assoc from 'ramda/src/assoc';
import lensProp from 'ramda/src/lensProp';
import forwardTo from 'flyd/module/forwardto';
import Union from 'union-type';
import * as Collapsible from 'Collapsible';

export const init = (heading) => ({
	valueList: [],
	collapsible: Collapsible.init(),
	heading
});

export const Action = Union({
	SetValueList: [Array],
	Collapsible: [Collapsible.Action]
});

export const update = Action.caseOn({
	SetValueList: assoc('valueList'),
	Collapsible: Collapsible.updateNested(lensProp('collapsible'))
});

export const render = (model, actions$) =>
	Collapsible.render(
		model.collapsible, forwardTo(actions$, Action.Collapsible),
		model.heading,
		renderValueList(model.valueList)
	);

const renderValueList = (valueList) =>
	h("table.pure-table.pure-table-bordered", [
		h("thead", h("tr", [h("th", "#"), h("th", "Type"), h("th", "Value")])),
		h("tbody", valueList.map((value, index) => {
			const parsedValue = parseValue(value);
			return h("tr", [
				h("td", index.toString()),
				h("td", parsedValue._name),
				h("td", renderValue(parsedValue))
			])
		}))
	]);

const Value = Union({
	Nil: [],
	Number: [Number],
	Boolean: [Boolean],
	String: [String],
	List: [Array],
	Symbol: [String],
	Native: [Number],
	Function: [Number],
	Placeholder: [Number],
	Corrupted: []
});

const parseValue = (rawValue) => {
	if(rawValue === null) {
		return Value.Nil();
	} else if(typeof rawValue == 'number') {
		return Value.Number(rawValue);
	} else if(typeof rawValue == 'boolean') {
		return Value.Boolean(rawValue);
	} else if(typeof rawValue == 'string') {
		return Value.String(rawValue);
	} else if(rawValue instanceof Array) {
		return Value.List(rawValue);
	} else if(rawValue["symbol"]) {
		return Value.Symbol(rawValue["symbol"]);
	} else if(rawValue["native"]) {
		return Value.Native(rawValue["native"]);
	} else if(rawValue["function"]) {
		return Value.Function(rawValue["function"]);
	} else if(rawValue["placeholder"]) {
		return Value.Placeholder(rawValue["placeholder"]);
	} else {
		return Value.Corrupted();
	}
};

const renderValue = Value.case({
	Nil: () => "nil",
	Number: (number) => number.toString(),
	Boolean: (boolean) => boolean.toString(),
	String: (string) => string,
	List: (list) => renderValueList(list),
	Symbol: (string) => string,
	Native: (number) => "0x" + number.toString(16),
	Function: (number) => "0x" + number.toString(16),
	Placeholder: (number) => number.toString(),
	Corrupted: () => ""
});
