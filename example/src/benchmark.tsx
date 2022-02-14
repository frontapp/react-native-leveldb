import {LevelDB} from "react-native-leveldb";
import {Text} from "react-native";
import * as React from "react";
import AsyncStorage from "@react-native-async-storage/async-storage";
import {compareReadWrite, getRandomString, getTestSetArrayBuffer, getTestSetString, getTestSetStringRecord} from "./test-util";

export interface BenchmarkResults {
  writeMany: { numKeys: number, durationMs: number }
  readMany: { numKeys: number, durationMs: number }
}

export function benchmarkLeveldb(): BenchmarkResults {
  let name = getRandomString(32) + '.db';
  console.info('Opening DB', name);
  const db = new LevelDB(name, true, true);

  let res: Partial<BenchmarkResults> = {};

  const writeKvs = getTestSetStringRecord(10000);
  const writeKeys = Object.keys(writeKvs);

  // === writeMany
  let started = new Date().getTime();
  db.putAllStr(writeKvs);

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

export async function benchmarkAsyncStorage(): Promise<BenchmarkResults> {
  console.info('Clearing AsyncStorage');
  try {
    await AsyncStorage.clear();
  } catch (e) {
    if (!e?.message?.includes('Failed to delete storage directory')) {
      throw e;
    }
  }

  let res: Partial<BenchmarkResults> = {};

  const writeKvs: [string, string][] = getTestSetString(10000);

  // === writeMany
  let started = new Date().getTime();
  await AsyncStorage.multiSet(writeKvs);
  res.writeMany = {numKeys: writeKvs.length, durationMs: new Date().getTime() - started};

  // === readMany
  started = new Date().getTime();
  const readKvs =
    await AsyncStorage.multiGet(await AsyncStorage.getAllKeys()) as [string, string][];
  res.readMany = {numKeys: readKvs.length, durationMs: new Date().getTime() - started};

  compareReadWrite(writeKvs, readKvs);
  return res as BenchmarkResults;
}

export const BenchmarkResultsView = (x: BenchmarkResults & { title: string }) => {
  const {writeMany, readMany, title} = x;
  const writeManyRes = writeMany &&
    `wrote ${writeMany.numKeys} items in ${writeMany.durationMs}ms; ` +
    `(${(writeMany.numKeys / writeMany.durationMs).toFixed(1)}items/ms)`;
  const readManyRes = readMany &&
    `read ${readMany.numKeys} items in ${readMany.durationMs}ms; ` +
    `(${(readMany.numKeys / readMany.durationMs).toFixed(1)}items/ms)`;

  return (<>
    <Text>== {title}</Text>
    <Text>Benchmark write many: {writeManyRes}</Text>
    <Text>Benchmark read many: {readManyRes}</Text>
  </>);
}