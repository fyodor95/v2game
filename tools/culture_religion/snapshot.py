"""Снимок культуры и религии собственных тестовых процессов без выполнения кода игры."""
from __future__ import annotations
import json
from pathlib import Path
import struct
from tools.chronology_import import native_event_probe as probe
from tools.chronology_import.engine_acceptance import require, work_path


def inspect(read,base,tags):
    def u32(at): return struct.unpack('<I',read(at,4))[0]
    def text(at):
        raw=read(at,24);length,capacity=struct.unpack_from('<II',raw,16)
        require(length<=capacity and length<=1024 and (capacity>=16 or length<16),'Некорректная строка')
        return read(at if capacity<16 else struct.unpack_from('<I',raw)[0],length).decode('cp1251') if length else ''
    manager=u32(base+0xe587e4);require(manager,'Нет manager')
    count,table=u32(manager),u32(manager+4)
    require(0<count<=65536 and table,'Границы manager')
    result={}
    for index in range(1,count):
        country=u32(table+index*4)
        if not country: continue
        packed=u32(country+0x1c);tag=struct.pack('<I',packed)[:3].decode('ascii',errors='replace')
        if tag not in tags: continue
        require(tag not in result and u32(country+0x20)==index,'Идентичность страны')
        primary,religion=u32(country+0xe64),u32(country+0xe68)
        require(primary and religion,'Нет primary/religion')
        result[tag]=dict(primary=text(primary+0x18),religion=text(religion+0x10),religion_index=u32(religion+8),
                         country_address=country,primary_address=primary,religion_address=religion)
    require(set(result)==set(tags),'Не все страны найдены')
    return result


def capture(environment,session,owned,tags):
    environment=work_path(environment);game=environment/'game'
    current=json.loads((environment/'session.json').read_text(encoding='utf-8'))
    require(current['token']==session['token'] and current['pid']==session['pid']==owned.pid and owned.handle,'Не подтверждено владение session')
    require(Path(current['game']).resolve()==game and owned.kernel.WaitForSingleObject(owned.handle,0)==258,'Чужая среда или процесс завершён')
    executable=(game/'v2game.exe').resolve()
    require(executable.is_relative_to(environment) and probe.engine.cli.digest(executable)==probe.engine.cli.EXE_HASH,'Неизвестный EXE')
    process=probe.ReadOnlyProcess(owned.pid,executable)
    try:
        module=process.main_module();base=module['base']
        pe=probe.pe_identity(process.read,base,executable.read_bytes())
        u32=lambda at:struct.unpack('<I',process.read(at,4))[0]
        world=u32(base+0xe588e8);require(world,'Нет загруженного мира')
        date=u32(world+0xb0c)
        state=inspect(process.read,base,tags)
        require(u32(base+0xe588e8)==world and u32(world+0xb0c)==date,'Мир или дата изменились во время снимка')
        return dict(date_raw=date,read_only=True,atomic_snapshot=False,pid=owned.pid,session_token=session['token'],pe=pe,countries=state)
    finally: process.close()
