import assert from 'node:assert/strict';
import { copyFile, readFile } from 'node:fs/promises';

const read = (path) => readFile(new URL(path, import.meta.url), 'utf8');
const config = JSON.parse(await read('./cydoc.config.json'));
const cmake = await read('../CMakeLists.txt');
assert.equal(config.version, cmake.match(/^project\(CyMCPHub VERSION (\S+) /m)?.[1]);
const data = '../Documentation/DocData';
const manifest = JSON.parse(await read(data + '/manifest.json'));
const report = JSON.parse(await read(data + '/translation-report.json'));
for (const key of ['missing', 'outdated', 'untracked']) assert.equal(report[key].length, 0, key);
for (const id of new Set(manifest.pages.map(page => page.id))) {
    const pages = manifest.pages.filter(page => page.id === id);
    assert.deepEqual(pages.map(page => page.locale).sort(), ['en', 'fr'], id);
    const anchors = page => page.headings.map(heading => heading.id).sort();
    assert.deepEqual(anchors(pages[0]), anchors(pages[1]), id);
}
const service = await read('../src/hub_service.cpp');
const toolNames = Array.from(service.matchAll(/tools->add\(\{"([^"]+)"/g), match => match[1]);
assert.ok(toolNames.length > 0);
for (const locale of ['en', 'fr']) {
    const reference = await read('./manual/' + locale + '/mcp.md');
    for (const name of toolNames) assert.ok(reference.includes('`' + name + '`'), locale + ': ' + name);
}
await copyFile(new URL('./vendor/LICENSE', import.meta.url), new URL(data + '/CyDoc.LICENSE.txt', import.meta.url));
console.log('Verified the application version, both languages, matching anchors and public MCP tool names.');
