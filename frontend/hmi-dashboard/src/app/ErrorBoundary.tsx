import React from 'react';

interface ErrorBoundaryState {
  error: Error | null;
}

export class ErrorBoundary extends React.Component<React.PropsWithChildren, ErrorBoundaryState> {
  state: ErrorBoundaryState = { error: null };

  static getDerivedStateFromError(error: Error): ErrorBoundaryState {
    return { error };
  }

  render() {
    if (this.state.error) {
      return (
        <main className="startup-error">
          <section>
            <h1>Frontend Runtime Error</h1>
            <p>{this.state.error.message}</p>
            <pre>{this.state.error.stack}</pre>
          </section>
        </main>
      );
    }

    return this.props.children;
  }
}
