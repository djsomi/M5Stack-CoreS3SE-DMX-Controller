"""SYNTHETIC, NOT INSTALLABLE stable-contract tests; no accepted images packaged.

Only image header interpretation is stubbed for synthetic bytes. Real partition
and candidate range/checksum gates, notice material and deterministic ZIP code run.
No production override exists. No Git commits/refs or device data are changed.
"""
import copy
import hashlib
import json
from pathlib import Path
import shutil
from types import SimpleNamespace
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts'))
import assemble_stable_release as stable
import firmware_layout as layout


class StableTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='SYNTHETIC-ONLY-stable-',dir=ROOT/'dist')
        self.addCleanup(self.temp.cleanup); self.root=Path(self.temp.name)
        inputs=self.root/'inputs';inputs.mkdir()
        args=SimpleNamespace(tooling_root=ROOT,tooling_commit='c'*40,publication_root=inputs/'publication',
            source=inputs/'frozen',builds=inputs/'builds',framework=inputs/'framework',esptool=inputs/'esptool',
            source_archive=inputs/'source.zip',notice_archive=inputs/'notices.zip',materials_tree=inputs/'materials',
            notices_tree=inputs/'legal',relink_evidence=inputs/'proof.json',output=self.root/'outputs',check_only=False)
        for p in [args.publication_root,args.framework/'tools/partitions',args.notices_tree]:p.mkdir(parents=True)
        sdk=Path.home()/'.platformio/packages/framework-arduinoespressif32/tools/gen_esp32part.py'
        shutil.copyfile(sdk,args.framework/'tools/gen_esp32part.py')
        (args.framework/'tools/partitions/boot_app0.bin').write_bytes(b'SYNTHETIC OTA NOT INSTALLABLE')
        for p in [args.source_archive,args.notice_archive]:
            with zipfile.ZipFile(p,'w') as z:z.writestr('SYNTHETIC-NOT-INSTALLABLE.txt',p.name)
        self.enterContext(patch.object(stable,'SOURCE_SHA256',stable.file_sha(args.source_archive)))
        self.enterContext(patch.object(stable,'NOTICE_SHA256',stable.file_sha(args.notice_archive)))
        self.enterContext(patch.object(stable.candidate,'validate_image',side_effect=lambda p,e:Path(p).read_bytes()))
        docs={n:b'# SYNTHETIC PUBLIC DOCUMENT; NOT INSTALLABLE\n' for n in stable.DOCUMENTS}
        for n,data in docs.items():
            p=args.publication_root/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
        notice_rows=stable.publication.inventory.verify_repository(ROOT)
        legal_names=[*stable.notices.FILES,stable.publication.inventory.MANIFEST]+[
            r['path']for r in notice_rows['notice_files']if r['id']!='freefont-preferred-source']
        legal={n:stable.notices.canonical_file(ROOT/n)for n in legal_names}
        for n,data in legal.items():
            p=args.notices_tree/n;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
        profiles=[];guards={args.source_archive:stable.SOURCE_SHA256,args.notice_archive:stable.NOTICE_SHA256}
        for env in stable.candidate.PROFILES:
            folder=args.builds/env;folder.mkdir(parents=True)
            provenance=dict(schema=1,version='0.3.0',target_release='0.3.0',status='stable',source_modified=False,
                source_commit=stable.frozen.COMMIT,source_sha256=stable.frozen.SOURCE_SHA256,environment=env,
                build_id='0.3.0+SYNTHETIC-NOT-INSTALLABLE')
            body=b'SYNTHETIC FIRMWARE DO NOT FLASH\n'+('\n'.join(str(v)for v in provenance.values())).encode()
            sdk_module=layout.partition_sdk(args.framework)
            table=sdk_module.PartitionTable.from_csv((ROOT/'partitions/cores3se_16mb_ota.csv').read_text())
            data={'firmware.bin':body,'bootloader.bin':b'SYNTHETIC BOOT DO NOT FLASH',
                'partitions.bin':table.to_binary(),'boot_app0.bin':(args.framework/'tools/partitions/boot_app0.bin').read_bytes()}
            components=[]
            for n,offset in stable.candidate.BINARIES.items():
                p=folder/n;p.write_bytes(data[n]);guards[p]=stable.sha(data[n])
                components.append(dict(file=n,offset=offset,size=len(data[n]),sha256=stable.sha(data[n]),
                    erase_range=[offset//4096*4096,(offset+len(data[n])+4095)//4096*4096]))
            receipt=dict(profile=env,diagnostics=False,layout=layout.LAYOUT_ID,flash_size=0x1000000,
                components=components,provenance=provenance)
            (folder/'infrastructure.json').write_bytes(stable.dump(receipt))
            guards[folder/'infrastructure.json']=stable.file_sha(folder/'infrastructure.json')
            profiles.append(receipt)
        tooling=dict(commit='c'*40,publication_base=stable.PUBLICATION,files=[],source_sha256=stable.sha(stable.dump([])),
            runtime=dict(python='synthetic-runtime',zlib='synthetic'),zip=dict(compression='DEFLATE',level=9))
        pub=dict(commit=stable.PUBLICATION,input_sha256=stable.PUBLICATION_SHA256,dirty=False)
        documents=[dict(path=n,bytes=len(docs[n]),sha256=stable.sha(docs[n]))for n in stable.candidate.RELEASE_DOCUMENTS]
        report=dict(firmware=dict(tag=stable.frozen.TAG,tag_object=stable.frozen.TAG_OBJECT,
            source_commit=stable.frozen.COMMIT,source_sha256=stable.frozen.SOURCE_SHA256),publication=pub,
            profiles=profiles,publication_ready=True,distribution_documentation=documents)
        self.ctx=dict(args=args,tooling=tooling,preflight=report,documents=docs,notices=legal,guards=guards,
            document_manifest=stable.named_payload(docs),notice_manifest=stable.notice_manifest(ROOT))
        self.args=args

    def build(self):
        # Explicit unittest injection only: production always runs real gates.
        with patch.object(stable,'verify_inputs',return_value=self.ctx),patch.object(stable,'unchanged'):
            return stable.assemble(self.args)

    def test_complete_unit_and_base_and_deterministic_release(self):
        first=self.build();stable.verify_release(self.args.output,self.ctx)
        original={p.name:p.read_bytes()for p in self.args.output.iterdir()}
        self.args.output=self.root/'outputs-again';second=self.build()
        self.assertEqual(original,{p.name:p.read_bytes()for p in self.args.output.iterdir()})
        self.assertEqual(first,second)
        for env,label in stable.candidate.PROFILES.items():
            with zipfile.ZipFile(self.args.output/f'CoreS3SE-DMX-v0.3.0-{label}.zip') as z:
                m=json.loads(z.read('manifest.json'))
                self.assertFalse(m['release_candidate']);self.assertEqual(m['classification'],'stable-release')
                self.assertEqual(m['firmware']['source_commit'],stable.frozen.COMMIT)
                self.assertEqual(m['publication']['commit'],stable.PUBLICATION)
                self.assertEqual(m['assembler']['commit'],'c'*40)
                self.assertEqual(stable.sha(z.read('BUILD-RECEIPT.json')),m['receipt_sha256'])
                self.assertEqual(m['hardware_status'],'recommended'if env.endswith('unit')else'experimental')
                self.assertIn('unverified',m['migration']['base'])
                for n in stable.DOCUMENTS:self.assertEqual(z.read(n),self.ctx['documents'][n])
                for n in self.ctx['notices']:self.assertEqual(z.read(n),self.ctx['notices'][n])
                self.assertIn(b'SYNTHETIC FIRMWARE DO NOT FLASH',z.read('firmware.bin'))

    def test_wrong_firmware_publication_and_assembler_identity(self):
        for section,field,value in [('firmware','source_commit','0'*40),('publication','commit','0'*40),
            ('publication','input_sha256','0'*64)]:
            ctx=copy.deepcopy(self.ctx);ctx['preflight'][section][field]=value
            with self.assertRaises(ValueError):stable.payload(ctx,'cores3se_unit')
        for field,value in [('commit',stable.PUBLICATION),('commit','d'*40),('publication_base','0'*40),('source_sha256','0'*64)]:
            ctx=copy.deepcopy(self.ctx);ctx['tooling'][field]=value
            with self.assertRaises(ValueError):stable.payload(ctx,'cores3se_unit')

    def test_modified_binary_and_receipt_and_candidate_receipt(self):
        image=self.args.builds/'cores3se_unit/firmware.bin';original=image.read_bytes();image.write_bytes(original+b'bad')
        with self.assertRaises(ValueError):stable.payload(self.ctx,'cores3se_unit')
        image.write_bytes(original)
        receipt=self.args.builds/'cores3se_unit/infrastructure.json';receipt_original=receipt.read_bytes()
        receipt.write_bytes(receipt_original+b'\n')
        with self.assertRaisesRegex(ValueError,'receipt changed'):stable.payload(self.ctx,'cores3se_unit')
        with patch.object(stable,'tooling_identity',return_value=self.ctx['tooling']),\
             patch.object(stable,'publication_identity',return_value=self.ctx['preflight']['publication']):
            with self.assertRaisesRegex(ValueError,'input changed'):stable.unchanged(self.ctx)
        receipt.write_bytes(receipt_original)
        for field,value in [('release_candidate',True),('release_candidate',False)]:
            ctx=copy.deepcopy(self.ctx);ctx['preflight']['profiles'][0][field]=value
            with self.assertRaises(ValueError):stable.payload(ctx,'cores3se_unit')
        ctx=copy.deepcopy(self.ctx);ctx['preflight']['profiles'][0]['provenance']['version']='0.3.0-rc.1'
        with self.assertRaises(ValueError):stable.payload(ctx,'cores3se_unit')

    def test_incorrect_offsets_and_sector_rounded_nvs_overlap(self):
        for value in [0x9000,0x8001,0x1000]:
            ctx=copy.deepcopy(self.ctx);ctx['preflight']['profiles'][0]['components'][0]['offset']=value
            with self.assertRaises(ValueError):stable.payload(ctx,'cores3se_unit')
        ctx=copy.deepcopy(self.ctx);ctx['preflight']['profiles'][0]['components'][0]['erase_range']=[0,0xe000]
        with self.assertRaises(ValueError):stable.payload(ctx,'cores3se_unit')

    def test_missing_or_modified_docs_notices_and_pending_metadata(self):
        expected=stable.payload(self.ctx,'cores3se_unit')
        for section,name in [('documents','KNOWN_ISSUES.md'),('notices','LICENSE')]:
            ctx=copy.deepcopy(self.ctx);ctx[section].pop(name)
            with self.assertRaises(ValueError):stable.payload(ctx,'cores3se_unit')
            ctx=copy.deepcopy(self.ctx);ctx[section]['private.log']=b'not allowed'
            with self.assertRaises(ValueError):stable.payload(ctx,'cores3se_unit')
        mutations=[]
        for n in stable.candidate.RELEASE_DOCUMENTS:
            mutations.extend([(n,None),(n,expected[n]+b'bad')])
        for n in stable.notices.FILES:mutations.append((n,None))
        m=json.loads(expected['manifest.json']);m['release_candidate']=True
        mutations.append(('manifest.json',stable.dump(m)))
        m=json.loads(expected['manifest.json']);m['physical_migration_validation']='pending'
        mutations.append(('manifest.json',stable.dump(m)))
        for i,(n,data) in enumerate(mutations):
            changed=copy.deepcopy(expected)
            if data is None:changed.pop(n)
            else:changed[n]=data
            changed['SHA256SUMS.txt']=''.join(stable.sha(d)+'  '+n+'\n'for n,d in sorted(changed.items())if n!='SHA256SUMS.txt').encode()
            path=self.root/f'SYNTHETIC-tampered-{i}.zip';stable.write_zip(path,changed)
            with self.assertRaises(ValueError):stable.verify_bundle(path,expected)

    def test_duplicate_private_unsafe_and_unexpected_archive_members(self):
        files=stable.payload(self.ctx,'cores3se_unit')
        for i,n in enumerate(['manifest.json','private/NVS.bin','../escape','docs/FT6336U_THDIFF_ABA_TEST.md']):
            path=self.root/f'SYNTHETIC-extra-{i}.zip';stable.write_zip(path,files)
            with zipfile.ZipFile(path,'a') as z:z.writestr(n,b'NOT PERMITTED')
            with self.assertRaises(ValueError):stable.verify_bundle(path,files)

    def test_zip_encoding_and_release_directory_integrity(self):
        self.build();expected=stable.payload(self.ctx,'cores3se_unit')
        path=self.args.output/'CoreS3SE-DMX-v0.3.0-UNIT.zip';body=path.read_bytes()
        path.write_bytes(body+b'trailing garbage')
        with self.assertRaises(ValueError):stable.verify_bundle(path,expected)
        path.write_bytes(body);(self.args.output/'private.txt').write_bytes(b'private')
        with self.assertRaises(ValueError):stable.verify_release(self.args.output,self.ctx)
        (self.args.output/'private.txt').unlink()
        p=self.args.output/'RELEASE.json';original=p.read_bytes();p.write_bytes(original+b' ')
        with self.assertRaises(ValueError):stable.verify_release(self.args.output,self.ctx)
        p.write_bytes(original);p=self.args.output/'SHA256SUMS.txt';p.write_bytes(b'bad')
        with self.assertRaises(ValueError):stable.verify_release(self.args.output,self.ctx)

    def test_source_notice_pair_tampering_and_missing_mandatory_license(self):
        self.args.source_archive.write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError,'source/notice archive'):stable.payload(self.ctx,'cores3se_unit')
        p=self.args.notices_tree/'LICENSE';p.unlink()
        with self.assertRaises((ValueError,OSError)):
            stable.publication.verify_notice_tree(ROOT,self.args.notices_tree,ROOT)

    def test_no_overwrite_check_only_and_fail_before_output(self):
        self.build();before={p.name:p.read_bytes()for p in self.args.output.iterdir()}
        with self.assertRaises(FileExistsError):self.build()
        self.assertEqual(before,{p.name:p.read_bytes()for p in self.args.output.iterdir()})
        self.args.output=self.root/'check-only';self.args.check_only=True;self.build()
        self.assertFalse(self.args.output.exists())
        self.args.check_only=False;self.args.tooling_commit=stable.PUBLICATION
        with self.assertRaises(ValueError):stable.assemble(self.args)
        self.assertFalse(self.args.output.exists())

    def test_current_production_tooling_gate_and_explicit_commit_required(self):
        with self.assertRaises(ValueError):stable.tooling_identity(ROOT,stable.PUBLICATION)
        with self.assertRaises(SystemExit):stable.parser().parse_args([])

    def fake_tooling(self,dirty=False,extra=b'',changes=None):
        names=[p.relative_to(ROOT).as_posix()for p in (ROOT/'scripts').glob('*.py')]+list(stable.TOOL_CHANGES[1:])
        data={n:stable.notices.canonical_file(ROOT/n)for n in names}
        ids={n:hashlib.sha1(b'blob '+str(len(d)).encode()+b'\0'+d).hexdigest()for n,d in data.items()}
        def fake(root,*args,**kw):
            if args==('rev-parse','HEAD'):return b'c'*40
            if args==('rev-parse','v0.3.0'):return stable.frozen.TAG_OBJECT.encode()
            if args==('rev-parse','v0.3.0^{commit}'):return stable.frozen.COMMIT.encode()
            if args[0]=='merge-base':return b''
            if args[:3]==('diff','HEAD','--name-only'):return b'scripts/assemble_stable_release.py'if dirty else b''
            if args[0]=='diff':return '\n'.join(changes or stable.TOOL_CHANGES).encode()
            if args[0]=='ls-files':return extra
            if args[:3]==('ls-tree','-r','--name-only'):return '\n'.join(names).encode()
            if args[0]=='ls-tree':return b''.join(('100644 blob '+ids[n]+'\t'+n).encode()+b'\0'for n in names)
            raise AssertionError(args)
        return fake,{ids[n]:('blob',d)for n,d in data.items()}

    def test_committed_T_identity_dirty_untracked_and_unrelated_change_rejection(self):
        for dirty,extra,changes,valid in [(False,b'',None,True),(True,b'',None,False),
            (False,b'scripts/private.py\0',None,False),(False,b'',list(stable.TOOL_CHANGES)+['src/src.cpp'],False)]:
            fake,blobs=self.fake_tooling(dirty,extra,changes)
            with patch.object(stable.frozen,'git',side_effect=fake),patch.object(stable.frozen,'batch',return_value=blobs):
                if valid:
                    identity=stable.tooling_identity(ROOT,'c'*40)
                    self.assertEqual(identity['commit'],'c'*40)
                    self.assertEqual(identity['source_sha256'],stable.sha(stable.dump(identity['files'])))
                else:
                    with self.assertRaises(ValueError):stable.tooling_identity(ROOT,'c'*40)

    def test_wrong_P_and_unverifiable_provenance_rejected(self):
        with patch.object(stable,'git',return_value='0'*40):
            with self.assertRaises(ValueError):stable.publication_identity(ROOT)
        with patch.object(stable,'git',side_effect=OSError('Git unavailable')):
            with self.assertRaises(OSError):stable.tooling_identity(ROOT,'c'*40)

    def test_loaded_helper_outside_T_and_wrong_expected_T_rejected(self):
        fake,blobs=self.fake_tooling()
        with patch.object(stable.frozen,'git',side_effect=fake),patch.object(stable.frozen,'batch',return_value=blobs):
            with self.assertRaises(ValueError):stable.tooling_identity(ROOT,'d'*40)
            with patch.object(stable,'loaded_modules',return_value=[self.root/'rogue.py']):
                with self.assertRaisesRegex(ValueError,'verified T'):stable.tooling_identity(ROOT,'c'*40)

    def test_framework_and_interpreter_tool_hash_lock(self):
        self.args.materials_tree.mkdir();self.args.esptool.mkdir()
        py=self.args.framework/'tools/gen_esp32part.py';original=py.read_bytes()
        m=dict(files=[dict(path='core/packages/framework-arduinoespressif32/tools/gen_esp32part.py',
            sha256=stable.sha(original))])
        tool=self.args.esptool/'synthetic-tool.py';tool.write_bytes(b'# synthetic tool\n')
        lock=dict(tools=[dict(name='tool-esptoolpy',files=stable.materials.file_rows(self.args.esptool))])
        (self.args.materials_tree/'BUILD-TOOLS.json').write_bytes(stable.dump(lock))
        with patch.object(stable.materials,'verify_tree',return_value=m):
            stable.external_inputs(self.args)
            py.write_bytes(original+b'bad')
            with self.assertRaisesRegex(ValueError,'framework differs'):stable.external_inputs(self.args)
            py.write_bytes(original);tool.write_bytes(b'bad')
            with self.assertRaisesRegex(ValueError,'esptool differs'):stable.external_inputs(self.args)

    def test_committed_document_blob_missing_modified_rejected(self):
        name='KNOWN_ISSUES.md';p=self.args.publication_root/name;original=p.read_bytes()
        oid=hashlib.sha1(b'blob '+str(len(original)).encode()+b'\0'+original).hexdigest()
        tree=('100644 blob '+oid+'\t'+name).encode()+b'\0'
        with patch.object(stable.frozen,'git',return_value=tree),\
             patch.object(stable.frozen,'batch',return_value={oid:('blob',original)}):
            stable.committed_files(self.args.publication_root,stable.PUBLICATION,[name])
            p.write_bytes(original+b'bad')
            with self.assertRaises(ValueError):stable.committed_files(self.args.publication_root,stable.PUBLICATION,[name])
            p.unlink()
            with self.assertRaises(ValueError):stable.committed_files(self.args.publication_root,stable.PUBLICATION,[name])


if __name__=='__main__':unittest.main(verbosity=2)
