/* eslint-disable react/no-did-mount-set-state */
import * as React from 'react';
import { StyleSheet, View, Text } from 'react-native';
import {
  benchmarkAsyncStorage,
  benchmarkJSONvsMPack,
  benchmarkLeveldb,
  BenchmarkResults,
  BenchmarkResultsView,
} from './benchmark';
import { leveldbExample, leveldbTests, leveldbMsgPack } from './example';

interface BenchmarkState {
  leveldb?: BenchmarkResults;
  mpack?: ReturnType<typeof benchmarkJSONvsMPack>;
  leveldbExample?: boolean;
  leveldbTests: string[];
  messagePack?: void;
  asyncStorage?: BenchmarkResults;
  error?: string;
}

export default class App extends React.Component<{}, BenchmarkState> {
  state: BenchmarkState = { leveldbTests: [] };

  componentDidMount() {
    this.setState({
      // leveldb: benchmarkLeveldb(),
      mpack: benchmarkJSONvsMPack(),
      // leveldbExample: leveldbExample(),
      // leveldbTests: leveldbTests(),
      messagePack: leveldbMsgPack(),
    });

    // benchmarkAsyncStorage().then((res) =>
    //   this.setState({ asyncStorage: res })
    // );
  }

  render() {
    return (
      <View style={styles.container}>
        <Text>
          Example validity:{' '}
          {this.state.leveldbExample === undefined
            ? ''
            : this.state.leveldbExample
            ? 'passed'
            : 'failed'}
        </Text>
        {this.state.leveldb && (
          <BenchmarkResultsView title="LevelDB" {...this.state.leveldb} />
        )}
        {this.state.mpack && (
          <>
            <BenchmarkResultsView title="MPACK" {...this.state.mpack} />
          </>
        )}
        {this.state.leveldbTests &&
          this.state.leveldbTests.map((msg, idx) => (
            <Text key={idx}>Test: {msg}</Text>
          ))}
        {this.state.asyncStorage && (
          <BenchmarkResultsView
            title="AsyncStorage"
            {...this.state.asyncStorage}
          />
        )}
        {this.state.error && <Text>ERROR RUNNING: {this.state.error}</Text>}
      </View>
    );
  }
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    justifyContent: 'center',
  },
});
