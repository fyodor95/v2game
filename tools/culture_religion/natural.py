"""Естественное срабатывание новой первой корки через существующий нативный стенд."""
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from tools.culture_religion.engine import install_adapters
from tools.chronology_import import natural_spawn


class NaturalRunner(natural_spawn.NaturalRunner):
    def checked(self,response,operation):
        result=super().checked(response,operation)
        if operation=='start-visible':
            from tools.culture_religion.native_acceptance import wait_initialisation
            self.record['tool_sources']['tools/culture_religion/natural.py']=natural_spawn.engine.cli.digest(Path(__file__))
            wait_initialisation(self.environment,(self.environment/'session.json').stat().st_mtime)
        return result

    def start_owned(self,game,previous):
        super().start_owned(game,previous)
        manifest=self.record['manifest']
        self.record['negative_event_checks']=[]
        # Проверяется настоящий province-scope evaluator, без исполнения опций.
        for name in ('religion','state_religion','civilization'):
            case=next(c for c in manifest['cases'] if c['name']==name)
            self.checked(natural_spawn.engine.cli.load_campaign(self.environment,case['save'],1200),
                         'negative-load '+case['save'])
            response=self.event_condition(manifest)
            natural_spawn.engine.require(response['trigger'] is False,'Неверный отрицательный province trigger: '+name)
            self.record['negative_event_checks'].append(dict(case=name,result=response))
            self.checkpoint()
            self.checked(natural_spawn.engine.cli.return_to_menu(self.environment,180),'negative-return-menu')


if __name__=='__main__':
    install_adapters()
    natural_spawn.NaturalRunner=NaturalRunner
    raise SystemExit(natural_spawn.main())
