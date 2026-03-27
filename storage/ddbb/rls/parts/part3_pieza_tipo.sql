-- ---------------------------------------------------------------------------
-- pieza_tipo (propiedad vía pieza.user_id)
-- ---------------------------------------------------------------------------
DROP POLICY IF EXISTS "pieza_tipo_select" ON public.pieza_tipo;
DROP POLICY IF EXISTS "pieza_tipo_insert" ON public.pieza_tipo;
DROP POLICY IF EXISTS "pieza_tipo_update" ON public.pieza_tipo;
DROP POLICY IF EXISTS "pieza_tipo_delete" ON public.pieza_tipo;

CREATE POLICY "pieza_tipo_select" ON public.pieza_tipo
  FOR SELECT TO authenticated
  USING (
    public.is_admin()
    OR (
      public.is_app_user()
      AND EXISTS (
        SELECT 1 FROM public.pieza p
        WHERE p.id = pieza_tipo.pieza_id AND p.user_id = auth.uid()
      )
    )
  );

CREATE POLICY "pieza_tipo_insert" ON public.pieza_tipo
  FOR INSERT TO authenticated
  WITH CHECK (
    public.is_admin()
    OR (
      public.is_app_user()
      AND EXISTS (
        SELECT 1 FROM public.pieza p
        WHERE p.id = pieza_tipo.pieza_id AND p.user_id = auth.uid()
      )
    )
  );

CREATE POLICY "pieza_tipo_update" ON public.pieza_tipo
  FOR UPDATE TO authenticated
  USING (
    public.is_admin()
    OR (
      public.is_app_user()
      AND EXISTS (
        SELECT 1 FROM public.pieza p
        WHERE p.id = pieza_tipo.pieza_id AND p.user_id = auth.uid()
      )
    )
  )
  WITH CHECK (
    public.is_admin()
    OR (
      public.is_app_user()
      AND EXISTS (
        SELECT 1 FROM public.pieza p
        WHERE p.id = pieza_tipo.pieza_id AND p.user_id = auth.uid()
      )
    )
  );

CREATE POLICY "pieza_tipo_delete" ON public.pieza_tipo
  FOR DELETE TO authenticated
  USING (
    public.is_admin()
    OR (
      public.is_app_user()
      AND EXISTS (
        SELECT 1 FROM public.pieza p
        WHERE p.id = pieza_tipo.pieza_id AND p.user_id = auth.uid()
      )
    )
  );
