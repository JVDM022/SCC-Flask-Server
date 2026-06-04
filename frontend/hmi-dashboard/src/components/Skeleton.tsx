export function Skeleton({ rows = 3 }: { rows?: number }) {
  return (
    <div className="skeleton-stack">
      {Array.from({ length: rows }, (_, index) => (
        <div className="skeleton-row" key={index} />
      ))}
    </div>
  );
}
