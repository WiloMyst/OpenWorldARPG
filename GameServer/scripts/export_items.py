# UE 编辑器内导出 DT_ItemDataInfo 全部行数据 -> GameServer/scripts/items_export.json
# 用法: Output Log 命令框 (Cmd 模式) 输入  py "D:/Triton/OpenWorldARPG/GameServer/scripts/export_items.py"
# 或无头: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=<本文件路径>
import unreal
import json

TABLE = '/Game/_Core/Data/DT_ItemDataInfo'
OUT = r'D:\Triton\OpenWorldARPG\GameServer\scripts\items_export.json'


def export_via_table_as_json(dt, out_path):
    # 官方整表序列化: 枚举/嵌套结构全覆盖, 优于逐字段 dir() dump
    text = unreal.DataTableFunctionLibrary.get_table_as_json(dt)
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(text)
    unreal.log('[export] GetTableAsJSON 写出 %d chars -> %s' % (len(text), out_path))


def export_row_by_row(dt, out_path):
    # 兜底路径: get_table_as_json 不可用时逐行 dump 基本字段与枚举
    result = []
    for n in unreal.DataTableFunctionLibrary.get_data_table_row_names(dt):
        row = unreal.DataTableFunctionLibrary.get_data_table_row(dt, n)
        if row is None:
            continue
        d = {'row_name': str(n)}
        for attr in dir(row):
            if attr.startswith('_'):
                continue
            try:
                v = getattr(row, attr)
            except Exception:
                continue
            if isinstance(v, (int, float, bool, str)):
                d[attr] = v
            elif isinstance(v, unreal.EnumBase) or isinstance(v, unreal.Text):
                d[attr] = str(v)
            else:
                try:
                    d[attr] = int(v)
                except Exception:
                    pass
        if not result:
            unreal.log('[export] ROW_ATTRS: %s' % [a for a in dir(row) if not a.startswith('_')])
        result.append(d)
    with open(out_path, 'w', encoding='utf-8') as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
    unreal.log('[export] 逐行导出 %d 行 -> %s' % (len(result), out_path))


def main():
    dt = unreal.load_asset(TABLE)
    if not dt:
        unreal.log_error('[export] 资产加载失败: %s' % TABLE)
        return
    names = unreal.DataTableFunctionLibrary.get_data_table_row_names(dt)
    unreal.log('[export] %s 共 %d 行: %s' % (TABLE, len(names), [str(n) for n in names]))
    try:
        export_via_table_as_json(dt, OUT)
    except Exception as e:
        unreal.log_warning('[export] get_table_as_json 失败 (%s), 回退逐行导出' % e)
        export_row_by_row(dt, OUT)


main()
