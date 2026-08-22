import unreal, json

ASSET = '/Game/_Core/Data/DT_ItemDataInfo'
OUT = r'D:\Triton\OpenWorldARPG\GameServer\scripts\items_export.json'

dt = unreal.load_asset(ASSET)
if dt is None:
    unreal.log_error('load asset failed: ' + ASSET)
else:
    names = unreal.DataTableFunctionLibrary.get_data_table_row_names(dt)
    result = []
    for n in names:
        row = unreal.DataTableFunctionLibrary.get_data_table_row(dt, n)
        if row is None:
            continue
        d = {'row_name': str(n)}
        # dump 所有可序列化基本字段 + 首行打印全部属性名
        for attr in dir(row):
            if attr.startswith('_'):
                continue
            try:
                v = getattr(row, attr)
            except Exception:
                continue
            tn = type(v).__name__
            if isinstance(v, (int, float, bool, str)):
                d[attr] = v
            elif tn in ('Int8', 'Int16', 'Int32', 'Int64', 'UInt8', 'UInt16', 'UInt32', 'UInt64'):
                d[attr] = int(v)
            elif attr in ('item_name', 'item_description', 'item_function_description'):
                d[attr] = str(v)
        if len(result) == 0:
            unreal.log('ROW_ATTRS: %s' % dir(row))
        result.append(d)

    with open(OUT, 'w', encoding='utf-8') as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
    unreal.log('exported %d rows -> %s' % (len(result), OUT))
