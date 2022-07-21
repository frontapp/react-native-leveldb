import { LevelDB } from '@frontapp/react-native-leveldb';
import { Text } from 'react-native';
import * as React from 'react';
import {
  compareReadWrite,
  getRandomString,
  getTestSetString,
  getTestSetStringRecord,
} from './test-util';
import _ from 'lodash';

export interface BenchmarkResults {
  writeMany: { numKeys: number; durationMs: number };
  readMany: { numKeys: number; durationMs: number };
}

export function benchmarkLeveldb(): BenchmarkResults {
  let name = getRandomString(32) + '.db';
  console.info('Opening DB', name);
  const db = new LevelDB(name, true, true);

  let res: Partial<BenchmarkResults> = {};

  const writeKvs = getTestSetStringRecord(1000);
  const writeKeys = Object.keys(writeKvs);

  // === writeMany
  let started = new Date().getTime();
  db.batchStr(writeKvs, []);

  res.writeMany = {
    durationMs: new Date().getTime() - started,
    numKeys: writeKeys.length,
  };

  // === readMany
  let readKvs: Record<string, string> = {};
  started = new Date().getTime();
  readKvs = db.getAllStr();
  res.readMany = {
    numKeys: Object.keys(readKvs).length,
    durationMs: new Date().getTime() - started,
  };
  db.close();

  writeKeys.forEach((element) => {
    if (writeKvs[element] !== readKvs[element]) {
      throw new Error('invalid read');
    }
  });

  return res as BenchmarkResults;
}

export function benchmarkJSONvsMPack(): BenchmarkResults {
  let dbNameMpack = getRandomString(32) + '.db';
  const dbMpack = new LevelDB(dbNameMpack, true, true);

  const toSave = getTestSetString(1000);

  const writeKvs: Record<string, any> = {
    content1: toSave,
    content2: toSave,
    content3: toSave,
  };
  const writeKeys = Object.keys(writeKvs);

  // === writeMany
  let started = new Date().getTime();
  dbMpack.batchObjects(writeKvs, []);

  const writeManyMpack = {
    durationMs: new Date().getTime() - started,
    numKeys: writeKeys.length,
  };

  // === readMany
  let readKvs: Record<string, string> = {};
  started = new Date().getTime();
  readKvs = dbMpack.getAllObjects();

  const readManyMpack = {
    numKeys: Object.keys(readKvs).length,
    durationMs: new Date().getTime() - started,
  };
  dbMpack.close();

  return {
    readMany: readManyMpack,
    writeMany: writeManyMpack,
  };
}

export const BenchmarkResultsView = (
  x: BenchmarkResults & { title: string }
) => {
  const { writeMany, readMany, title } = x;
  const writeManyRes =
    writeMany &&
    `wrote ${writeMany.numKeys} items in ${writeMany.durationMs}ms; ` +
      `(${(writeMany.numKeys / writeMany.durationMs).toFixed(1)}items/ms)`;
  const readManyRes =
    readMany &&
    `read ${readMany.numKeys} items in ${readMany.durationMs}ms; ` +
      `(${(readMany.numKeys / readMany.durationMs).toFixed(1)}items/ms)`;

  return (
    <>
      <Text>== {title}</Text>
      <Text>Benchmark write many: {writeManyRes}</Text>
      <Text>Benchmark read many: {readManyRes}</Text>
    </>
  );
};
