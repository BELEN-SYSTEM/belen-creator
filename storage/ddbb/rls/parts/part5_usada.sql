-- ---------------------------------------------------------------------------
-- usada
-- ---------------------------------------------------------------------------
DROP POLICY IF EXISTS "usada_select" ON public.usada;
DROP POLICY IF EXISTS "usada_insert" ON public.usada;
DROP POLICY IF EXISTS "usada_update" ON public.usada;
DROP POLICY IF EXISTS "usada_delete" ON public.usada;

CREATE POLICY "usada_select" ON public.usada
  FOR SELECT TO authenticated
  USING (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

CREATE POLICY "usada_insert" ON public.usada
  FOR INSERT TO authenticated
  WITH CHECK (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

CREATE POLICY "usada_update" ON public.usada
  FOR UPDATE TO authenticated
  USING (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()))
  WITH CHECK (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));

CREATE POLICY "usada_delete" ON public.usada
  FOR DELETE TO authenticated
  USING (public.is_admin() OR (public.is_app_user() AND user_id = auth.uid()));
