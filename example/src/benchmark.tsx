import { LevelDB } from '@frontapp/react-native-leveldb';
import { Text } from 'react-native';
import * as React from 'react';
import AsyncStorage from '@react-native-async-storage/async-storage';
import {
  compareReadWrite,
  getRandomString,
  getTestSetString,
  getTestSetStringRecord,
} from './test-util';
import { array } from './large-file';

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

export function benchmarkJSONvsMPack(): {
  mpack: BenchmarkResults;
  json: BenchmarkResults;
} {
  let dbNameMpack = getRandomString(32) + '.db';
  const dbMpack = new LevelDB(dbNameMpack, true, true);

  const writeKvs: Record<string, any> = {
    content1: array,
    content2: array,
    content3: array,
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

  let dbNameJSON = getRandomString(32) + '.db';
  const dbJSON = new LevelDB(dbNameJSON, true, true);

  // === writeMany
  started = new Date().getTime();
  dbJSON.batchStr(
    {
      content1: JSON.stringify(array),
      content2: JSON.stringify(array),
      content3: JSON.stringify(array),
    },
    []
  );

  const writeManyJSON = {
    durationMs: new Date().getTime() - started,
    numKeys: writeKeys.length,
  };

  // === readMany
  started = new Date().getTime();
  const read = dbJSON.getAllStr();
  readKvs = {
    content1: JSON.parse(read.content1),
    content2: JSON.parse(read.content2),
    content3: JSON.parse(read.content3),
  };

  const readManyJSON = {
    numKeys: Object.keys(readKvs).length,
    durationMs: new Date().getTime() - started,
  };
  dbJSON.close();

  return {
    mpack: { readMany: readManyMpack, writeMany: writeManyMpack },
    json: { readMany: readManyJSON, writeMany: writeManyJSON },
  };
}

export async function benchmarkAsyncStorage(): Promise<BenchmarkResults> {
  console.info('Clearing AsyncStorage');
  try {
    await AsyncStorage.clear();
  } catch (e: any) {
    if (!e?.message?.includes('Failed to delete storage directory')) {
      throw e;
    }
  }

  let res: Partial<BenchmarkResults> = {};

  const writeKvs: [string, string][] = getTestSetString(1000);

  // === writeMany
  let started = new Date().getTime();
  await AsyncStorage.multiSet(writeKvs);
  res.writeMany = {
    numKeys: writeKvs.length,
    durationMs: new Date().getTime() - started,
  };

  // === readMany
  started = new Date().getTime();
  const readKvs = (await AsyncStorage.multiGet(
    await AsyncStorage.getAllKeys()
  )) as [string, string][];

  res.readMany = {
    numKeys: readKvs.length,
    durationMs: new Date().getTime() - started,
  };

  compareReadWrite(writeKvs, readKvs);
  return res as BenchmarkResults;
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
