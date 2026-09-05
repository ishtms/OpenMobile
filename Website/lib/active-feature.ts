export function activeFeatureIndex(sectionTops: readonly number[], readingLine: number) {
  for (let index = sectionTops.length - 1; index > 0; index--) {
    if (sectionTops[index] <= readingLine) return index;
  }
  return 0;
}
