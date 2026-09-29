"""Reawakened content importer, bundled with the launcher as reawakened-import.exe.

Converts the optional content from the player's own games into a new output
folder; the launcher installs from there. Nothing is written elsewhere, and the
source games are only read.

  reawakened-import doom3  --game DIR --prey-base DIR [--prey-base DIR ...] --output DIR
  reawakened-import portal --game DIR --crowbar EXE --output DIR --work DIR
  reawakened-import prey   --prey-base DIR --output DIR

Doom 3 accepts the original game (with or without Resurrection of Evil) or the
BFG Edition. Portal accepts Steam (VPK) or unpacked copies. Prey builds the
files Reawakened derives from the player's imported Prey archives (the portal
gun's openings and the Jen seam repair); setup runs it after importing Prey.

Output protocol (stdout, one per line): "PROGRESS <0-100> <text>", then
"DONE <file count>" or "ERROR <message>". Exit code 0 on success.
"""
import argparse
import contextlib
import io
import shutil
import sys
import traceback
from pathlib import Path

HERE = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parent.parent))
for sub in ('doom3', 'portalgun', 'materials'):
    sys.path.insert(0, str(HERE/sub))


def report(percent, text):
    print(f'PROGRESS {percent} {text}', flush=True)


def fresh(folder):
    folder = Path(folder)
    if folder.exists():
        shutil.rmtree(folder)
    folder.mkdir(parents=True)
    return folder


def run_main(module, argv):
    """Run an importer's command-line main() with its own arguments."""
    saved = sys.argv
    sys.argv = [module.__file__] + [str(a) for a in argv]
    try:
        # Importer chatter goes to stderr so stdout stays machine-readable.
        with contextlib.redirect_stdout(sys.stderr):
            module.main()
    finally:
        sys.argv = saved


def import_doom3(args):
    output = fresh(args.output)
    report(5, 'Reading Doom 3')
    import import_shotgun
    argv = [args.game, output]
    for base in args.prey_base:
        argv += ['--prey-base', base]
    report(15, 'Converting weapons, sounds and textures')
    run_main(import_shotgun, argv + ['--save-compatible'])
    report(95, 'Checking converted files')
    if not (output/'doom3-import-manifest.json').is_file():
        raise RuntimeError('The Doom 3 import did not complete.')
    return output


def import_portal(args):
    output = fresh(args.output)
    work = fresh(args.work)
    if not Path(args.crowbar).is_file():
        raise RuntimeError('The model decompiler (Crowbar) is missing from this installation.')
    report(5, 'Reading Portal')
    import import_viewmodel
    report(15, 'Converting the portal gun model, textures and sounds')
    run_main(import_viewmodel, [args.game, args.crowbar, output, work])
    report(95, 'Checking converted files')
    if not (output/'models/reawakened/portalgun/view/view.md5mesh').is_file():
        raise RuntimeError('The Portal import did not complete.')
    shutil.rmtree(work, ignore_errors=True)
    return output


def build_prey(args):
    output = fresh(args.output)
    if not any(Path(args.prey_base).glob('pak00[0-6].pk4')):
        raise RuntimeError('The Prey data archives are missing.')
    report(10, 'Building the portal gun openings')
    import build_assets
    with contextlib.redirect_stdout(sys.stderr):
        build_assets.build(Path(args.prey_base), output)
    report(55, 'Building the Jen seam repair')
    import build_jen_seam
    run_main(build_jen_seam, ['--base', args.prey_base, '--output', output/'zz_reawakened_jen_seam.pk4'])
    report(95, 'Checking built files')
    for required in ('def/reawakened_portalgun_opening.def', 'models/reawakened/portalgun/blue.ase',
                     'zz_reawakened_jen_seam.pk4'):
        if not (output/required).is_file():
            raise RuntimeError('Building the Prey files did not complete.')
    return output


def main():
    parser = argparse.ArgumentParser(prog='reawakened-import')
    sub = parser.add_subparsers(dest='game_kind', required=True)
    d = sub.add_parser('doom3')
    d.add_argument('--game', required=True, type=Path)
    d.add_argument('--prey-base', required=True, action='append', type=Path)
    d.add_argument('--output', required=True, type=Path)
    p = sub.add_parser('portal')
    p.add_argument('--game', required=True, type=Path)
    p.add_argument('--crowbar', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--work', required=True, type=Path)
    r = sub.add_parser('prey')
    r.add_argument('--prey-base', required=True, type=Path)
    r.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    try:
        output = {'doom3': import_doom3, 'portal': import_portal, 'prey': build_prey}[args.game_kind](args)
        count = sum(1 for f in output.rglob('*') if f.is_file())
        report(100, 'Done')
        print(f'DONE {count}', flush=True)
        return 0
    except Exception as error:
        traceback.print_exc(file=sys.stderr)
        message = str(error).replace('\n', ' ') or type(error).__name__
        print(f'ERROR {message}', flush=True)
        return 1


if __name__ == '__main__':
    sys.exit(main())
