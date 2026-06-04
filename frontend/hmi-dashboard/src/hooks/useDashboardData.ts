import { useQuery } from '@tanstack/react-query';

import { fetchDashboardData } from '../api/client';

export function useDashboardData() {
  return useQuery({
    queryKey: ['dashboard-data'],
    queryFn: fetchDashboardData,
    refetchInterval: 3000,
    staleTime: 1800,
    retry: 1,
  });
}
