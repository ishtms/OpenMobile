import assert from 'node:assert/strict';
import test from 'node:test';
import { activeFeatureIndex } from './active-feature.ts';

test('keeps the current plugin selected until the next introduction reaches the reading line', () => {
  assert.equal(activeFeatureIndex([220, 1240, 2460], 180), 0);
  assert.equal(activeFeatureIndex([-920, 210, 1350], 180), 0);
  assert.equal(activeFeatureIndex([-950, 180, 1320], 180), 1);
});

test('follows jumps and reverse scrolling across sections of different lengths', () => {
  assert.equal(activeFeatureIndex([-4000, -2700, -1500, -100, 1200, 2400], 180), 3);
  assert.equal(activeFeatureIndex([-2400, -1100, 100, 1500, 2800, 4000], 180), 2);
  assert.equal(activeFeatureIndex([-1000, 300, 1500, 2900, 4200, 5400], 180), 0);
});

test('keeps the last plugin selected when scrolling beyond the collection', () => {
  assert.equal(activeFeatureIndex([-7400, -6100, -4900, -3500, -2200, -1000], 180), 5);
});
